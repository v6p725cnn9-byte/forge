#pragma once

#include <filesystem>

namespace forge {

std::filesystem::path executable_directory();
std::filesystem::path assets_directory();
std::filesystem::path scripts_directory();

} // namespace forge
