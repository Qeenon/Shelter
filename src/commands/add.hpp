#pragma once

struct add_command {
  bool show_help = false;
  std::string directory; // current directory?
  std::string action    = "pull";
  std::string branch    = "master";
  std::string upstream  = "origin master";
  std::string vcs       = "git";

  add_command(lyra::cli & cli) {
    cli.add_argument(
      lyra::command(
        "add", [this](const lyra::group & g) { this->do_command(g); })
        .help("Add directory.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .optional()
            .help("Target directory"))
        .add_argument(
          lyra::opt(action, "action")
            .name("-t").name("--task")
            .optional()
            .help("Action type"))
        .add_argument(
          lyra::opt(branch, "branch")
            .name("-b").name("--branch")
            .optional()
            .help("Target branch"))
        .add_argument(
          lyra::opt(upstream, "upstream")
            .name("-u").name("--upstream")
            .optional()
            .help("Target upstream"))
        .add_argument(
          lyra::opt(vcs, "vcs")
            .name("--vcs")
            .optional()
            .help("Target version control system (git)"))
    );
  }

  [[noreturn]] void
  do_command(const lyra::group & g) {
    if (directory.empty() || directory == ".") {
      directory = std::filesystem::current_path().generic_string();
    }
    if (show_help) {
      std::cout << g;
    } else {
      const auto& HomeDirectory = utils::get_home_dir();
      const std::string config_file = HomeDirectory + std::string("/") + CONFIG_FILE;
      if (std::filesystem::exists(config_file)) {
        auto config = YAML::LoadFile(config_file);
        YAML::Node new_node;
        new_node["target"]    = directory;
        new_node["task"]      = action;
        new_node["upstream"]  = upstream;
        new_node["branch"]    = branch;
        new_node["vcs"]       = vcs;
        new_node["hash"]      = "";
        config.push_back(new_node);
        save_config(config, config_file);
      }
    }
    exit(EXIT_SUCCESS);
  }
};
