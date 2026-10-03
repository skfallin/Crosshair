#include "core/pack.hpp"
#include "core/preset_json.hpp"
#include "apps/engine/storage.hpp"
#include "rendering/renderer.hpp"
#include <winrt/base.h>
#include <iostream>

using namespace crosshair;
int main() {
    try {
        winrt::init_apartment();
        Renderer renderer;
        Pack p{initialProfile(), starterPresets(), {}};
        const auto text = encodePack(p);
        if (encodePack(decodePack(text)) != text)
            throw std::runtime_error("Geometry pack round trip");
        const auto directory = std::filesystem::temp_directory_path() /
                               (L"CrosshairNative.PackTest." + std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directories(directory);
        const auto png = directory / L"image.png";
        const auto surface = renderer.rasterize(starterPresets()[0]);
        renderer.writePng(png, 257, 257, surface.bgra);
        const auto decoded = renderer.readPng(png);
        if (decoded.width != 257 || decoded.height != 257 || decoded.bgra != surface.bgra)
            throw std::runtime_error("PNG alpha round trip");
        auto bytes = readJsonFile(png, 16 * 1024 * 1024);
        p.assets.push_back({"image", {bytes.begin(), bytes.end()}});
        Layer image;
        image.shape = Shape::png;
        image.assetId = "image";
        p.presets[0].layers = {image};
        const auto encoded = encodePack(p);
        if (encodePack(decodePack(encoded)) != encoded)
            throw std::runtime_error("PNG pack round trip");
        auto reject = [](const Pack& bad) {
            try {
                validatePack(bad);
            } catch (const std::exception&) {
                return;
            }
            throw std::runtime_error("Hostile pack accepted");
        };
        auto bad = p;
        bad.presets[0].permission = std::string(1025, 'x');
        reject(bad);
        bad = p;
        bad.assets[0].id = "../outside";
        reject(bad);
        bad = p;
        bad.assets.push_back(bad.assets[0]);
        reject(bad);
        bad = p;
        bad.assets.clear();
        reject(bad);
        bad = p;
        bad.assets[0].png[16] = 0x7f;
        reject(bad);
        bad = p;
        bad.presets[0].layers[0].width = 100000;
        reject(bad);
        bad = p;
        bad.profile.defaultPresetId = "absent";
        reject(bad);
        bool refused = false;
        try {
            decodePack("[[[[[[[[[[]]]]]]]]]]");
        } catch (const std::exception&) {
            refused = true;
        }
        if (!refused)
            throw std::runtime_error("Deep JSON accepted");
        std::vector<std::uint8_t> oversized(2049 * 4, 0);
        const auto large = directory / L"large.png";
        renderer.writePng(large, 2049, 1, oversized);
        refused = false;
        try {
            renderer.readPng(large);
        } catch (const std::exception&) {
            refused = true;
        }
        if (!refused)
            throw std::runtime_error("Oversized decoded PNG accepted");
        std::filesystem::remove(png);
        std::filesystem::remove(large);
        std::filesystem::remove(directory);
        std::cout << "Geometry/PNG pack round trips, alpha, references, duplicate/traversal IDs, JSON depth "
                     "and decoded size limits passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
