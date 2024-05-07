#pragma once

#include "repository.hpp"

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

#ifdef _WIN32
#pragma warning( push )
#pragma warning ( disable : 4100 )
#pragma warning ( disable : 4458 )
#endif

#ifdef _WIN32
#pragma warning( pop )
#endif

#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <functional>

static const char* OPTIONS_FILE = ".shelter_options.yml";
static const char* CONFIG_FILE  = ".shelter.yml";

namespace {
  template <VCS T> const auto
  makeRepo = [] (const RepoArgs& args, const std::string& hash_str) {
    return std::make_shared<Repo<T>>(args, hash_str);
  };

  static std::unordered_map< std::string
       , std::function<std::shared_ptr<Repository>
              (RepoArgs args, std::string hash_str)> > const
  VCSTYPE =
    { { "git",        makeRepo<VCS::GitShell> }
    , { "pijul",      makeRepo<VCS::Pijul>    }
    , { "git shell",  makeRepo<VCS::GitShell> }
    };
}

[[nodiscard]] const std::vector<std::shared_ptr<Repository>>
parse_config(const YAML::Node& config) {
  std::vector<std::shared_ptr<Repository>> result;
  result.reserve(config.size());
  for (const auto& node : config) {
    const auto& targetNode    = node["target"];
    const auto& taskNode      = node["task"];
    const auto& upstreamNode  = node["upstream"];
    const auto& branchNode    = node["branch"];
    if (targetNode && taskNode && upstreamNode && branchNode) {
      const RepoArgs args(
        targetNode    .as<std::string>(),
        taskNode      .as<std::string>(),
        upstreamNode  .as<std::string>(),
        branchNode    .as<std::string>()
      );

      std::string hash_str;
      const auto& hashNode = node["hash"];
      if (hashNode) {
        hash_str = hashNode.as<std::string>();
      }

      if(const auto& vcsNode = node["vcs"]) {
        const auto vcs = vcsNode.as<std::string>();
        const auto& it = VCSTYPE.find(vcs);
        if (it != VCSTYPE.end()) {
          result.push_back( it->second(args, hash_str) );
        } else {
          std::cout << "unknown vcs specified: "
                    << vcs
                    << ", ignoring"
                    << std::endl;
        }
      } else {
        result.push_back(
          std::make_shared<Repository>(args, hash_str)
        );
      }
    }
  }
  return result;
}

void
save_config(YAML::Node& config, const std::string& conf) {
  std::ofstream fout(conf);
  fout << config;
  fout.flush();
  std::cout << "saving config" << std::endl;
  fout.close();
}
