#include "utils.hpp"
#include "config.hpp"

#ifndef _WIN32
#include "vcs/libgit.hpp"
#endif
#include "vcs/gitshell.hpp"
#include "vcs/pijul.hpp"

#include "lyra/lyra.hpp"

#include "commands/show.hpp"
#include "commands/add.hpp"
#include "commands/rm.hpp"

// You can't cout just dfine variable, you need to do #x thing
#define STRINGIFY(x) #x
// You can't just use #x thing you need to convert it into macro
// The extra level of indirection causes the value of the macro to be stringified instead of the name of the macro.
#define STRINGIFY_M(x) STRINGIFY(x)

void
show_version(const bool display_git_stats = false) {
  #ifdef VERSION_CMAKE
  std::cout << "Shelter v" << STRINGIFY_M(VERSION_CMAKE) << std::endl;
  #endif
  if (display_git_stats) {
    #if defined(BRANCH_CMAKE) && defined(HASH_CMAKE)
      std::cout << "Git branch: " << STRINGIFY_M(BRANCH_CMAKE)
                << ", Commit: "   << STRINGIFY_M(HASH_CMAKE)
                <<  std::endl;
    #endif
  }
}

int
main(int argc, char *argv[]) {
  auto verbose  = false;
  auto help     = false;
  auto version  = false;
  auto cli
    = lyra::cli()
    | lyra::help(help)
    | lyra::opt(verbose)
      ["-v"]["--verbose"]
      ("Display verbose output")
    | lyra::opt(
      [&](bool){ 
        version = true;
      })
      ["--version"]
      ("Display version")
    ;

  show_command _show { cli };
  add_command _add { cli };
  rm_command _rm { cli };

  const auto result = cli.parse( { argc, argv } );
  if ( !result ) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    return 1;
  }

  show_version(version);
  if (version) {
    return EXIT_SUCCESS;
  }

  if (help) {
    std::cout << "\n" << cli << std::endl;
    return EXIT_SUCCESS;
  }

  const auto& HomeDirectory = utils::get_home_dir() + std::string("/");

  const auto options_file = HomeDirectory + OPTIONS_FILE;
  const auto config_file  = HomeDirectory + CONFIG_FILE;

  std::shared_ptr<GlobalOptions> otpions = std::make_shared<GlobalOptions>();

  if (std::filesystem::exists(options_file)) {
    otpions->parse_options(options_file);
  }

  if (verbose) {
    otpions->set_verbose(true);
  }

  if (std::filesystem::exists(config_file)) {
    auto config = YAML::LoadFile(config_file);
    const auto& repositories = parse_config(config);

    bool some_hash_was_updated = false;
    for (auto& repo : repositories) {
      std::cout << "processing: " << repo << std::endl;
      repo->process(otpions);
      if (repo->is_hash_updated()) {
        for (auto it = config.begin(); it != config.end(); ++it) {
          if ((*it)["target"]
           && (*it)["target"].as<std::string>() == repo->target()) {
            (*it)["hash"] = repo->repo_hash();
            if (!some_hash_was_updated) {
              some_hash_was_updated = true;
            }
          }
        }
      }
    }

    if (some_hash_was_updated) {
      save_config(config, config_file);
    }

  } else {
    std::cout << "missing config: "
              << config_file
              << std::endl;
  }

  return EXIT_SUCCESS;
}
