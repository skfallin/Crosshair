#include "apps/engine/storage.hpp"
#include "core/preset_json.hpp"
#include <windows.h>
#include <winrt/base.h>
#include <fstream>
#include <iostream>

using namespace crosshair;
int main() {
    winrt::init_apartment();
    const auto directory = std::filesystem::temp_directory_path() /
                           (L"CrosshairNative.StorageTest." + std::to_wstring(GetCurrentProcessId()));
    try {
        auto p = initialProfile();
        for (const auto& preset : starterPresets()) {
            const auto encoded = encodePreset(preset);
            if (encodePreset(decodePreset(encoded)) != encoded)
                throw std::runtime_error("Preset round trip");
        }
        bool badPreset = false;
        try {
            decodePreset("{\"schemaVersion\":999}");
        } catch (const std::exception&) {
            badPreset = true;
        }
        if (!badPreset)
            throw std::runtime_error("Future preset accepted");
        p.name = "Profilo prova à";
        p.executablePath = L"C:\\Prova\\Gioco.exe";
        p.bindings.push_back({{Device::mouse, 4, 0}, 0, true, Action::slot, 2});
        const auto json = encodeProfile(p);
        if (encodeProfile(decodeProfile(json)) != json)
            throw std::runtime_error("Round trip");
        auto reject = [](std::string_view text) {
            try {
                decodeProfile(text);
            } catch (const std::exception&) {
                return;
            }
            throw std::runtime_error("Invalid document accepted");
        };
        reject("{}");
        reject(std::string(65537, ' '));
        reject("[[[[[[[[[[]]]]]]]]]]");
        auto future = json;
        const auto version = future.find("\"schemaVersion\":1");
        if (version == std::string::npos)
            throw std::runtime_error("Missing version");
        future.replace(version, 17, "\"schemaVersion\":2");
        reject(future);
        for (const auto id : {"../outside", "C:drive", "bad/name", "with.dot", "UPPER", ""}) {
            bool rejected = false;
            try {
                profileFile(directory, id);
            } catch (const std::exception&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Unsafe profile ID accepted");
        }
        auto second = p;
        second.id = "second";
        second.name = "Secondo profilo";
        const auto secondFile = profileFile(directory, second.id);
        saveProfile(secondFile, second);
        if (savedProfiles(directory).size() != 1 || savedProfiles(directory)[0].id != "second")
            throw std::runtime_error("Profile collection");
        std::filesystem::remove(secondFile);
        const auto file = directory / L"profile.json";
        saveProfile(file, p);
        p.name = "Seconda revisione";
        saveProfile(file, p);
        if (loadProfile(file).profile.name != p.name)
            throw std::runtime_error("Save/load");
        {
            std::ofstream damaged(file);
            damaged << "{broken";
        }
        const auto recovered = loadProfile(file);
        if (recovered.writable || recovered.warning.empty() || recovered.profile.name != "Profilo prova à")
            throw std::runtime_error("Backup recovery");
        std::ifstream original(file);
        std::string content;
        std::getline(original, content);
        original.close();
        if (content != "{broken")
            throw std::runtime_error("Corrupt file was not preserved");
        std::filesystem::remove(file);
        std::filesystem::remove(file.wstring() + L".bak");
        std::filesystem::remove(directory);
        std::cout << "Profile round trip, limits, future schema, atomic replacement and recovery passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
