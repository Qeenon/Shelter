#pragma once

#include "stdafx.hpp"

namespace utils {

[[nodiscard]] const char*
get_home_dir() {
  #ifdef unix
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpedantic"
  #pragma GCC diagnostic ignored "-Wnarrowing"
    const static volatile char A = 'a';
    const char HOME[5] = {A-25, A-18, A-20, A-28, 0};
    const auto HomeDirectory = getenv(HOME);
  #pragma GCC diagnostic pop
  #elif defined(_WIN32)
    char* HomeDirectory;
    size_t required_size;
    getenv_s( &required_size, NULL, 0, "USERPROFILE");
    if (required_size == 0) {
      std::cout << "USERPROFILE env doesn't exist!" << std::endl;
    }
    HomeDirectory = (char*) malloc(required_size * sizeof(char));
    if (!HomeDirectory) {
      std::cout <<("Failed to allocate memory!\n");
    }
    getenv_s( &required_size, HomeDirectory, required_size, "USERPROFILE" );
  #else
    auto HomeDirectory = ".";
  #endif
  return HomeDirectory;
}

}
