#pragma once
#include "core/model.hpp"
#include <filesystem>

namespace crosshair {
struct LoadedProfile {
    Profile profile;
    std::string warning;
    bool writable = true;
};
std::filesystem::path dataDirectory();
void useTestDataDirectory(std::string_view token);
void checkJsonLimits(std::string_view text, std::size_t maximumBytes = 65536);
std::string encodeProfile(const Profile& profile);
Profile decodeProfile(std::string_view json);
LoadedProfile loadProfile(const std::filesystem::path& path);
void saveProfile(const std::filesystem::path& path, const Profile& profile);
std::filesystem::path profileFile(const std::filesystem::path& directory, std::string_view id);
std::vector<Profile> savedProfiles(const std::filesystem::path& directory);
std::string readJsonFile(const std::filesystem::path& path, std::size_t maximumBytes = 65536);
void writeJsonFile(const std::filesystem::path& path, std::string_view json,
                   std::size_t maximumBytes = 65536);
void rejectReparsePath(const std::filesystem::path& path);
} // namespace crosshair
