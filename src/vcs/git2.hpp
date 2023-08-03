#pragma once

#include <git2.h>

#include "repository.hpp"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace {
  const std::string get_remote_hash(const std::string& upstream) {
    const std::string ls_remote_cmd = "git ls-remote " + upstream;
    std::string ls_remote = exec(ls_remote_cmd.c_str());
    std::string::size_type tpos = ls_remote.find('\t');
    if (tpos != std::string::npos) {
      return ls_remote.substr(0, tpos);
    }
    return ls_remote;
  }
  void clean(git_repository* repo, bool verbose = false) {
    git_object* target = nullptr;
    int error = git_revparse_single(&target, repo, "HEAD");
    if (error != 0) {
      std::cout << "git_revparse_single error code: " << error << std::endl;
      return;
    }
    error = git_reset(repo, target, GIT_RESET_HARD, NULL);
    if (error != 0) {
      std::cout << "git_reset error code: " << error << std::endl;
      git_object_free(target);
      return;
    }
    git_object_free(target);

    const auto output = exec("git clean -fxd");
    if (verbose) {
      std::cout << output << std::endl;
    }
  }
}

template <> void Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  git_libgit2_init();

  const auto& repo_branch = branch();
  const auto& repo_path = target();

  git_repository* repo = nullptr;
  int error = git_repository_open(&repo, repo_path.data());
  if (error < 0) {
    std::cout << "libgit2 repository open error code: " << error << std::endl;
    git_libgit2_shutdown();
    return;
  }

  git_reference* head_ref = nullptr;
  error = git_repository_head(&head_ref, repo);
  if (error < 0) {
    std::cout << "libgit2 repository head error code: " << error << std::endl;
    git_repository_free(repo);
    git_libgit2_shutdown();
    return;
  }

  const char* branch_name = nullptr;
  error = git_branch_name(&branch_name, head_ref);
  if (error < 0) {
    std::cout << "libgit2 branch name error code: " << error << std::endl;
    git_repository_free(repo);
    git_libgit2_shutdown();
    return;
  }

  if (branch_name != repo_branch) {
    error = git_reference_dwim(&head_ref, repo, repo_branch.c_str());
    if (error < 0) {
      std::cout << "git_reference_dwim error code: " << error << std::endl;
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }
    git_checkout_options gcopts = GIT_CHECKOUT_OPTIONS_INIT;
    error = git_checkout_tree(repo, (const git_object*)git_reference_target(head_ref), &gcopts);
    if (error < 0) {
      std::cout << "git_checkout_tree error code: " << error << std::endl;
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }
    if (opts->is_verbose()) {
      std::cout << "checkout to " << repo_branch << " complete" << std::endl;
    }
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    git_oid commit_oid;
    const git_oid *commit_head_oid = git_reference_target(head_ref);
    git_oid_cpy(&commit_oid, commit_head_oid);

    char commit_hash[GIT_OID_HEXSZ + 1];
    git_oid_fmt(commit_hash, &commit_oid);

    local_hash = commit_hash;
    set_hash( local_hash );
  }

  const auto& repo_upstream = upstream();
  std::vector<std::string> upstream_split;
  std::stringstream ss(repo_upstream);
  std::string token;

  while (getline(ss, token, ' ')) {
    upstream_split.push_back(token);
  }

  std::string_view upstream_remote = upstream_split[0];

  git_remote* remote = nullptr;
  error = git_remote_lookup(&remote, repo, upstream_remote.data());
  if (error < 0) {
    std::cout << "git_remote_lookup error code: " << error << std::endl;
    git_repository_free(repo);
    git_libgit2_shutdown();
    return;
  }

  error = git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL);
  bool connected = false;
  std::string remote_hash;
  if (error < 0) {
    remote_hash = get_remote_hash( repo_upstream );
  } else {
    connected = true;
    const git_remote_head **refs;
    size_t refs_len;
    error = git_remote_ls(&refs, &refs_len, remote);
    if (error < 0) {
      std::cout << "git_remote_ls error code: " << error << std::endl;
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }
    char oid[GIT_OID_SHA1_HEXSIZE + 1] = {0};
    git_oid_fmt(oid, &refs[0]->oid);
    remote_hash = oid;
  }

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    clean(repo, opts->is_verbose());
  }

  if (connected) {
    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);
    if (error != 0) {
      std::cout << "git_remote_fetch error code: " << error << std::endl;
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }

    std::string upstream_branch =
      upstream_split.size() > 1 ? upstream_split[1]
                                : branch_name;

    std::string upstream;
    upstream.reserve(upstream_remote.size() + upstream_branch.size() + 1);
    upstream.append(upstream_remote);
    upstream.push_back('/');
    upstream.append(upstream_branch);

    git_reference* branch_ref = nullptr;
    error = git_branch_lookup(&branch_ref, repo, upstream.c_str(), GIT_BRANCH_REMOTE);
    if (error != 0) {
      std::cout << "git_branch_lookup error code: " << error << std::endl;
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }

    git_annotated_commit* commit = nullptr;
    error = git_annotated_commit_from_ref(&commit, repo, branch_ref);
    if (error != 0) {
      std::cout << "git_annotated_commit_from_ref error code: " << error << std::endl;
      git_reference_free(branch_ref);
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }

    const git_oid* commit_oid = git_annotated_commit_id(commit);

    git_object* commit_object = nullptr;
    int error = git_object_lookup(&commit_object, repo, commit_oid, GIT_OBJECT_COMMIT);
    if (error != 0) {
      std::cout << "git_object_lookup error code: " << error << std::endl;
      git_reference_free(branch_ref);
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
    }

    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    error = git_checkout_tree(repo, commit_object, &checkout_opts);
    if (error != 0) {
      std::cout << "git_checkout_tree error code: " << error << std::endl;
      git_reference_free(branch_ref);
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }

    git_reference* local_branch_ref = nullptr;
    error = git_branch_lookup(&local_branch_ref, repo, branch_name, GIT_BRANCH_LOCAL);
    if (error != 0) {
      std::cout << "git_branch_lookup for local error code: " << error << std::endl;
      git_reference_free(branch_ref);
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
    }

    git_reference *new_target_ref;
    error = git_reference_set_target(&new_target_ref, local_branch_ref, commit_oid, nullptr);
    if (error != 0) {
      std::cout << "git_reference_set_target for local error code: " << error << std::endl;
      git_reference_free(local_branch_ref);
      git_reference_free(branch_ref);
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
    }

    git_reference_free(new_target_ref);

    error = git_branch_set_upstream(local_branch_ref, upstream.c_str());
    if (error != 0) {
      std::cout << "git_branch_set_upstream error code: " << error << std::endl;
      git_reference_free(local_branch_ref);
      git_reference_free(branch_ref);
      git_remote_free(remote);
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }

    git_annotated_commit_free(commit);
    git_reference_free(local_branch_ref);
    git_reference_free(branch_ref);
  } else {
    const auto pull_cmd = "git pull " + repo_upstream;
    const auto output = exec(pull_cmd.c_str());
    if (opts->is_verbose()) {
      std::cout << output << std::endl;
    }
  }

  git_repository_state_cleanup(repo);

  set_hash( remote_hash );

  git_remote_free(remote);
  git_reference_free(head_ref);
  git_repository_free(repo);

  git_libgit2_shutdown();
}

template <> void Repo <VCS::Git> :: rebase (
  const std::shared_ptr<GlobalOptions>& opts
) {
  git_libgit2_init();

  const auto& repo_branch = branch();
  const auto& repo_path = target();

  git_repository* repo = nullptr;
  int error = git_repository_open(&repo, repo_path.data());
  if (error < 0) {
    std::cout << "libgit2 repository open error code: " << error << std::endl;
    git_libgit2_shutdown();
    return;
  }

  git_reference* head_ref = nullptr;
  error = git_repository_head(&head_ref, repo);
  if (error < 0) {
    std::cout << "libgit2 repository head error code: " << error << std::endl;
    git_repository_free(repo);
    git_libgit2_shutdown();
    return;
  }

  const char* branch_name = nullptr;
  error = git_branch_name(&branch_name, head_ref);
  if (error < 0) {
    std::cout << "libgit2 branch name error code: " << error << std::endl;
    git_repository_free(repo);
    git_libgit2_shutdown();
    return;
  }

  if (branch_name != repo_branch) {
    error = git_reference_dwim(&head_ref, repo, repo_branch.c_str());
    if (error < 0) {
      std::cout << "git_reference_dwim error code: " << error << std::endl;
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }
    git_checkout_options gcopts = GIT_CHECKOUT_OPTIONS_INIT;
    error = git_checkout_tree(repo, (const git_object*)git_reference_target(head_ref), &gcopts);
    if (error < 0) {
      std::cout << "git_checkout_tree error code: " << error << std::endl;
      git_repository_free(repo);
      git_libgit2_shutdown();
      return;
    }
    if (opts->is_verbose()) {
      std::cout << "checkout to " << repo_branch << " complete" << std::endl;
    }
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    git_oid commit_oid;
    const git_oid *commit_head_oid = git_reference_target(head_ref);
    git_oid_cpy(&commit_oid, commit_head_oid);

    char commit_hash[GIT_OID_HEXSZ + 1];
    git_oid_fmt(commit_hash, &commit_oid);

    local_hash = commit_hash;
    set_hash( local_hash );
  }

  const auto& repo_upstream = upstream();
  const auto remote_hash = get_remote_hash( repo_upstream );

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    clean(repo, opts->is_verbose());
  }

  std::string pull_cmd = "git pull --rebase " + repo_upstream;
  const auto pull_output = exec(pull_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << pull_output << std::endl;
  }

  std::string push_cmd = "git push --force origin " + repo_branch;
  const auto push_output = exec(push_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << push_output << std::endl;
  }

  set_hash( remote_hash );

  git_reference_free(head_ref);
  git_repository_free(repo);

  git_libgit2_shutdown();
}
