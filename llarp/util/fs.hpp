#pragma once
#include <fstream>
#ifdef USE_GHC_FILESYSTEM
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#else
#include <filesystem>
namespace fs
{
    using namespace std::filesystem;
    using ifstream = std::ifstream;
    using ofstream = std::ofstream;
    using fstream = std::fstream;
}  // namespace fs
#endif
