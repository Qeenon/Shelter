#pragma once

#include <tuple>

struct list_command {
  bool show_help = false;
  std::string directory;

  list_command(lyra::cli & cli) {
    cli.add_argument(
      lyra::command(
        "list", [this](const lyra::group & g) { this->do_command(g); })
        .help("Show repositories.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .optional()
            .help("Target directory"))
    );
  }

  [[noreturn]] void
  do_command(const lyra::group & g) {
    if (show_help) {
      std::cout << g;
    } else {
      const auto& HomeDirectory = utils::get_home_dir();
      const std::string config_file = HomeDirectory + std::string("/") + CONFIG_FILE;
      if (std::filesystem::exists(config_file)) {
        auto config = YAML::LoadFile(config_file);
        if (directory.empty()) {
          size_t max_target_len = 0;
          size_t max_branch_len = 0;
          std::vector<
            std::tuple<
              std::tuple< std::string, size_t >,
              std::tuple< std::string, size_t >,
              std::string
            >
          > strings;
          strings.reserve(config.size());
          for(YAML::Node node : config) {
            const auto& target_str      = node["target"].as<std::string>();
            const auto& branch_str      = node["branch"].as<std::string>();
            const auto& task_str        = node["task"].as<std::string>();
            const auto& target_str_size = target_str.size();
            const auto& branch_str_size = branch_str.size();
            max_target_len = std::max(max_target_len, target_str_size);
            max_branch_len = std::max(max_branch_len, branch_str_size);
            strings.push_back(
              std::make_tuple( std::make_tuple( target_str, target_str_size )
                             , std::make_tuple( branch_str, branch_str_size )
                             , task_str )
            );
          }
          for (auto&& s_node: strings) {
            std::tuple< std::string, size_t > target_str_tuple, branch_str_tuple;
            std::string target_str, branch_str, task_str;
            size_t target_str_size, branch_str_size;
            std::tie(target_str_tuple, branch_str_tuple, task_str) = s_node;
            std::tie(target_str, target_str_size) = target_str_tuple;
            std::tie(branch_str, branch_str_size) = branch_str_tuple;
            std::string target_str_stacer(max_target_len - target_str_size, ' ');
            std::string branch_str_stacer(max_branch_len - branch_str_size, ' ');
            std::cout << target_str << target_str_stacer
                      << " (" << branch_str << ")" << branch_str_stacer
                      << " [" << task_str << "]" << std::endl;
          }
        } else {
          bool found = false;
          for(YAML::Node node : config) {
            const auto& target_str = node["target"].as<std::string>();
            if (directory == target_str) {
              const auto& task_str   = node["task"]  .as<std::string>();
              const auto& branch_str = node["branch"].as<std::string>();
              std::cout << target_str
                        << " (" << branch_str << ")"
                        << " [" << task_str << "]" << std::endl;
              found = true;
            }
          }
          if (!found) {
            std::cout << "repository " << directory << " not found" << std::endl;
          }
        }
      }
    }
    exit(EXIT_SUCCESS);
  }
};
