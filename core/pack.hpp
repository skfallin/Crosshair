#pragma once
#include "core/model.hpp"

namespace crosshair {
constexpr std::size_t maxPackBytes = 100 * 1024 * 1024;
struct PackAsset {
    std::string id;
    std::vector<std::uint8_t> png;
};
struct Pack {
    Profile profile;
    std::vector<Preset> presets;
    std::vector<PackAsset> assets;
};
void validatePack(const Pack& pack);
std::string encodePack(const Pack& pack);
Pack decodePack(std::string_view text);
} // namespace crosshair
