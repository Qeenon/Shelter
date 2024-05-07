#pragma once

#include "options.hpp"
#include "execute.hpp"

#include <filesystem>
#include <unordered_map>

enum class [[nodiscard]] VCS { Git, Pijul, GitShell };
enum class [[nodiscard]] Action { Pull, Rebase, Unkown };

static std::unordered_map<std::string, Action> const
STRACTION =
  { { "pull",    Action::Pull   }
  , { "rebase",  Action::Rebase }
  };

static std::unordered_map<std::string, std::string> const
MIGMA =
  { { ".migma.py",    "python" }
  , { ".migma.sh",    "bash" }
  , { ".migma.raku",  "raku" }
  , { ".migma.pl",    "perl" }
  };

std::ostream& operator
<< (std::ostream& os, const Action& a) {
  auto it = std::find_if( std::begin(STRACTION), std::end(STRACTION)
                        , [&a](auto&& p) { return p.second == a; } );

  if (it == std::end(STRACTION)) {
    os << "Unknown ("
       << static_cast<std::underlying_type<Action>::type>(a)
       << ")";
  } else {
    os << it->first;
  }

  return os;
}

class [[nodiscard]] RepoArgs {
  std::string target;
  std::string upstream;
  std::string branch;
  Action action;
  public:

  RepoArgs(std::string t, std::string a, std::string u, std::string b)
    : target(t), upstream(u), branch(b) {
    const auto& it = STRACTION.find(a);
    if (it != STRACTION.end()) {
      action = it->second;
    } else {
      action = Action::Unkown;
    }
  }

  friend class Repository;
};

class [[nodiscard]] Repository {
  RepoArgs args;
  std::string hash;
  bool hash_updated;

  [[nodiscard]] bool
  navigate() const {
    if (std::filesystem::exists(args.target)) {
      std::filesystem::current_path(args.target);
      return true;
    }
    return false;
  }

  void
  migma(const std::shared_ptr<GlobalOptions>& opts) const {
    const std::filesystem::path targetPath(args.target);
    if (std::filesystem::exists(targetPath)) {
      for (const auto& m : MIGMA) {
        const std::filesystem::path migmaFile = targetPath / m.first;
        if (std::filesystem::exists(migmaFile)) {
          std::stringstream migmaCmd;
          migmaCmd << m.second << " " << m.first;
          const auto output = exec(migmaCmd.str().c_str());
          if (opts->is_verbose()) {
            std::cout << output << std::endl;
          }
          break;
        }
      }
    }
  }

  virtual void
  pull   (const std::shared_ptr<GlobalOptions>&) {};

  virtual void
  rebase (const std::shared_ptr<GlobalOptions>&) {};

  public:
  Repository(const RepoArgs& a, const std::string& h)
    : args(a), hash(h), hash_updated(false) {};

  virtual ~Repository() {};

  [[nodiscard]] const std::string_view
  target() const          { return args.target;   }

  [[nodiscard]] const std::string&
  upstream() const        { return args.upstream; }

  [[nodiscard]] const std::string&
  branch() const          { return args.branch;   }

  [[nodiscard]] const std::string&
  repo_hash() const       { return hash;          }

  [[nodiscard]] const bool&
  is_hash_updated() const { return hash_updated;  }

  void
  set_hash(const std::string& h) {
    if (hash != h) {
      std::cout << "new hash: " << h << std::endl;
      hash = h;
      hash_updated = true;
    }
  }

  void
  process(const std::shared_ptr<GlobalOptions>& opts) {
    if (navigate()) {
      switch (args.action) {
        [[likely]] case Action::Pull: {
          pull(opts);
          break;
        }
        case Action::Rebase: {
          rebase(opts);
          break;
        }
        [[unlikely]] case Action::Unkown: {
          std::cout << "unknown task for " << this << std::endl;
          return;
        }
      }
      if (is_hash_updated()) {
        migma(opts);
      }
    }
  }

  [[nodiscard]] const
  std::string details() {
    std::stringstream details;
    details << args.target
            << " (" << args.branch
            << ") [" << args.action << "]";
    return details.str();
  }

  friend std::ostream& operator
  << (std::ostream& os, const Repository& r) {
    os << r.target();
    return os;
  }

  friend std::ostream& operator
  << (std::ostream& os, const Repository* r) {
    os << r->target();
    return os;
  }
};

template <VCS G>
class [[nodiscard]] Repo final : public Repository {
  virtual void
  pull (const std::shared_ptr<GlobalOptions>&);

  virtual void
  rebase (const std::shared_ptr<GlobalOptions>&);

  public:
  Repo(const RepoArgs& a, const std::string& h)
    : Repository(a, h) {};
};
