#pragma once
#include "core/model.hpp"
#include <filesystem>

namespace crosshair {
std::string encodePreset(const Preset& preset);
Preset decodePreset(std::string_view text);
std::filesystem::path installedDirectory();
Preset loadPreset(std::string_view id);
} // namespace crosshair
