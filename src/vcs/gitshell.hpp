#pragma once

#include <ranges>

#include "repository.hpp"

namespace {
  const std::string
  shell_get_local_hash() {
    std::string log_hash = exec("git log -n 1 --pretty=format:%H");
    log_hash.erase(std::remove(log_hash.begin(), log_hash.end(), '\n'), log_hash.end());
    return log_hash;
  }
  const std::string
  shell_get_remote_hash(const std::string& upstream) {
    const std::string ls_remote_cmd = "git ls-remote " + upstream;
    std::string ls_remote = exec(ls_remote_cmd.c_str());
    std::string::size_type tpos = ls_remote.find('\t');
    if (tpos != std::string::npos) {
      return ls_remote.substr(0, tpos);
    }
    return ls_remote;
  }
  void
  shell_clean(bool verbose = false) {
    const auto output1 = exec("git reset --hard");
    const auto output2 = exec("git clean -fxd");
    if (verbose) {
      std::cout << output1 << "\n"
                << output2 << std::endl;
    }
  }
}

namespace gitshell {
  const std::string
  get_branch() {
    std::string rev_parse = exec("git rev-parse --abbrev-ref HEAD");
    rev_parse.erase(std::remove(rev_parse.begin(), rev_parse.end(), '\n'), rev_parse.end());
    return rev_parse;
  }
}

template <> void
Repo <VCS::GitShell> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto& repo_branch = branch();
  if (gitshell::get_branch() != repo_branch) {
    const auto checkout_cmd = "git checkout " + repo_branch;
    const auto output = exec(checkout_cmd.c_str());
    if (opts->is_verbose()) {
      std::cout << output << std::endl;
    }
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    local_hash = shell_get_local_hash();
    set_hash(local_hash);
  }

  const auto& repo_upstream = upstream();
  const auto remote_hash = shell_get_remote_hash(repo_upstream);

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    shell_clean(opts->is_verbose());
  }

  const auto pull_cmd = "git pull " + repo_upstream;
  const auto output = exec(pull_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << output << std::endl;
  }

  set_hash( remote_hash );
}

template <> void
Repo <VCS::GitShell> :: rebase (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto& repo_branch = branch();
  if (gitshell::get_branch() != repo_branch) {
    const auto checkout_cmd = "git checkout " + repo_branch;
    const auto output = exec(checkout_cmd.c_str());
    if (opts->is_verbose()) {
      std::cout << output << std::endl;
    }
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    local_hash = shell_get_local_hash();
    set_hash( local_hash );
  }

  const auto& repo_upstream = upstream();
  const auto remote_hash = shell_get_remote_hash(repo_upstream);

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    shell_clean(opts->is_verbose());
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
}
