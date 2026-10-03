#include "core/preset_json.hpp"
#include "apps/engine/storage.hpp"
#include "rendering/renderer.hpp"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <iomanip>
#include <numbers>

using namespace crosshair;
using namespace winrt;
using namespace winrt::Windows::Data::Json;
namespace {
Layer line(float x, float y, float ex, float ey, float thickness = 1) {
    return {Shape::line, x, y, ex, ey, 2, thickness, 1, {1, 1, 1, 1}};
}
Layer ring(float radius, float thickness = 1) {
    return {Shape::ring, 0, 0, 0, 0, radius, thickness, 1, {1, 1, 1, 1}};
}
Layer dot(float x, float y, float radius = 1) {
    return {Shape::dot, x, y, 0, 0, radius, 1, 1, {1, 1, 1, 1}};
}
std::string geometry(const Preset& p) {
    std::vector<std::string> layers;
    for (const auto& l : p.layers) {
        std::ostringstream s;
        s.imbue(std::locale::classic());
        s << std::fixed << std::setprecision(4) << static_cast<int>(l.shape) << ':' << l.outline << ':';
        if (l.shape == Shape::line) {
            auto a = std::pair{l.x, l.y}, b = std::pair{l.endX, l.endY};
            if (b < a)
                std::swap(a, b);
            s << a.first << ',' << a.second << ',' << b.first << ',' << b.second << ',' << l.thickness;
        } else {
            s << l.x << ',' << l.y << ',' << l.radius;
            if (l.shape == Shape::ring)
                s << ',' << l.thickness;
        }
        layers.push_back(s.str());
    }
    std::sort(layers.begin(), layers.end());
    std::string result;
    for (const auto& l : layers)
        result += l + ';';
    return result;
}
// Keep old IDs loadable for existing profiles, but never list their size variants.
std::vector<Preset> legacyPresets() {
    auto presets = starterPresets();
    presets[0].family = "punti";
    presets[1].family = "croci";
    presets[2].family = "combinazioni";
    const std::array<std::string, 8> families{"punti",   "croci",   "anelli", "cerchi",
                                              "forme-t", "chevron", "aperti", "combinazioni"};
    for (int family = 0; family < 8; ++family)
        for (int i = 0; i < 40; ++i) {
            Preset p;
            p.provenance = "geometria originale del progetto";
            p.source = "tools/catalog-builder/main.cpp";
            std::ostringstream id;
            id << "original-" << families[family] << '-' << std::setw(2) << std::setfill('0') << i + 1;
            p.id = id.str();
            p.name = families[family] + " " + std::to_string(i + 1);
            p.family = families[family];
            p.tags = {"contorno", family == 0 || family == 3 ? "centro pieno" : "centro libero"};
            const auto a = static_cast<float>(i % 8), b = static_cast<float>(i / 8);
            if (family == 0) {
                p.layers.push_back(dot(0, 0));
                const int count = 2 + i / 8;
                for (int k = 0; k < count; ++k) {
                    const auto angle = 2 * std::numbers::pi_v<float> * k / count;
                    p.layers.push_back(dot((4 + 2 * a) * std::cos(angle), (4 + 2 * a) * std::sin(angle)));
                }
            }
            if (family == 1) {
                const float x = 6 + 3 * a, y = 6 + 4 * b;
                p.layers = {line(-x, 0, x, 0), line(0, -y, 0, y)};
            }
            if (family == 2)
                p.layers = {ring(4 + 2 * a, 1 + b)};
            if (family == 3)
                p.layers = {ring(8 + 3 * a), dot(0, 0, .5f + .5f * b)};
            if (family == 4) {
                const float gap = 3 + 2 * b, end = gap + 6 + 2 * a;
                p.layers = {line(-end, 0, -gap, 0), line(gap, 0, end, 0), line(0, gap, 0, end)};
            }
            if (family == 5) {
                const float x = 4 + 4 * b, y = 4 + 3 * a;
                p.layers = {line(-x, y, 0, 0), line(0, 0, x, y)};
            }
            if (family == 6) {
                const float gap = 4 + 2 * b, end = gap + 5 + 3 * a;
                for (float x : {-1.f, 1.f})
                    for (float y : {-1.f, 1.f}) {
                        p.layers.push_back(line(x * gap, y * end, x * end, y * end));
                        p.layers.push_back(line(x * end, y * end, x * end, y * gap));
                    }
            }
            if (family == 7) {
                const float r = 8 + 3 * a, end = r + 3 + 3 * b;
                p.layers = {ring(r), line(-end, 0, -r - 2, 0), line(r + 2, 0, end, 0),
                            line(0, -end, 0, -r - 2), line(0, r + 2, 0, end)};
            }
            presets.push_back(std::move(p));
        }
    return presets;
}
void verifyGeometryControls(const Preset& original, Renderer& renderer) {
    auto edited = original;
    const auto check = [&](bool valid) {
        if (!valid)
            throw std::runtime_error("Independent geometry controls failed: " + original.id);
    };
    const auto closeEnough = [](float a, float b) { return std::abs(a - b) < .001f; };
    if (auto arms = armGeometry(edited)) {
        arms->horizontalGap = arms->verticalGap = 0;
        setArmGeometry(edited, *arms);
        check(armGeometry(edited).has_value());
        arms->horizontalGap = 7;
        arms->verticalGap = 3;
        setArmGeometry(edited, *arms);
        const auto actual = armGeometry(edited);
        check(actual && closeEnough(actual->horizontalLength, arms->horizontalLength) &&
              closeEnough(actual->verticalLength, arms->verticalLength) && actual->horizontalGap == 7 &&
              actual->verticalGap == 3);
    } else if (auto chevron = chevronGeometry(edited)) {
        chevron->gap = 5;
        setChevronGeometry(edited, *chevron);
        auto actual = chevronGeometry(edited);
        check(actual && closeEnough(actual->length, chevron->length) && closeEnough(actual->angle, chevron->angle) &&
              actual->gap == 5);
        chevron->gap = 0;
        chevron->length = 18;
        setChevronGeometry(edited, *chevron);
        actual = chevronGeometry(edited);
        check(actual && closeEnough(actual->length, 18) && actual->gap == 0);
    } else if (auto corners = cornerGeometry(edited)) {
        corners->horizontalGap = corners->verticalGap = 0;
        setCornerGeometry(edited, *corners);
        check(cornerGeometry(edited).has_value());
        corners->horizontalGap = 3;
        corners->verticalGap = 7;
        setCornerGeometry(edited, *corners);
        const auto actual = cornerGeometry(edited);
        check(actual && closeEnough(actual->horizontalLength, corners->horizontalLength) &&
              closeEnough(actual->verticalLength, corners->verticalLength) && actual->horizontalGap == 3 &&
              actual->verticalGap == 7);
    } else {
        check(std::all_of(edited.layers.begin(), edited.layers.end(), [](const Layer& l) {
            return l.shape == Shape::dot || (l.shape == Shape::ring && l.x == 0 && l.y == 0);
        }));
        if (dotSpacing(edited) > 0) {
            setDotSpacing(edited, .5f);
            setDotSpacing(edited, 20);
            check(closeEnough(dotSpacing(edited), 20));
        }
    }
    for (std::size_t i = 0; i < original.layers.size(); ++i) {
        const auto& before = original.layers[i];
        const auto& after = edited.layers[i];
        check(before.radius == after.radius && before.thickness == after.thickness &&
              before.outline == after.outline && before.color.a == after.color.a);
    }
    check(!validate(edited));
    const auto json = encodePreset(edited);
    auto reloaded = decodePreset(json);
    check(encodePreset(reloaded) == json);
    check(renderer.rasterize(edited).bgra == renderer.rasterize(reloaded).bgra);
}
} // namespace
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "Usage: catalog_builder OUTPUT_DIRECTORY\n";
        return 2;
    }
    try {
        init_apartment();
        Renderer renderer;
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output / L"presets");
        std::filesystem::create_directories(output / L"thumbnails");
        for (const auto& p : legacyPresets()) {
            verifyGeometryControls(p, renderer);
            writeJsonFile(output / L"presets" / ("preset-" + p.id + ".json"), encodePreset(p));
        }
        const auto& presets = templatePresets();
        std::map<std::string, std::string> unique;
        JsonArray index;
        constexpr unsigned cell = 128, columns = 16, width = cell * columns;
        const unsigned height = cell * static_cast<unsigned>((presets.size() + columns - 1) / columns);
        std::vector<std::uint8_t> sheet(static_cast<std::size_t>(width) * height * 4, 32);
        for (std::size_t k = 3; k < sheet.size(); k += 4)
            sheet[k] = 255;
        for (std::size_t i = 0; i < presets.size(); ++i) {
            const auto& p = presets[i];
            verifyGeometryControls(p, renderer);
            if (auto e = validate(p))
                throw std::runtime_error(*e);
            const auto [previous, inserted] = unique.emplace(geometry(p), p.id);
            if (!inserted)
                throw std::runtime_error("Geometry duplicate: " + p.id + " / " + previous->second);
            const auto json = encodePreset(p);
            if (encodePreset(decodePreset(json)) != json)
                throw std::runtime_error("Preset round trip failed");
            std::ofstream file(output / L"presets" / ("preset-" + p.id + ".json"), std::ios::binary);
            file << json;
            if (!file)
                throw std::runtime_error("Catalog write failed");
            const auto surface = renderer.rasterize(p);
            std::vector<std::uint8_t> thumb(128 * 128 * 4);
            for (unsigned y = 0; y < 128; ++y)
                for (unsigned x = 0; x < 128; ++x) {
                    const auto source = ((y + 64) * 257 + x + 64) * 4, target = (y * 128 + x) * 4;
                    std::copy_n(surface.bgra.begin() + source, 4, thumb.begin() + target);
                    const auto dest = (((i / columns) * cell + y) * width + (i % columns) * cell + x) * 4;
                    const auto alpha = thumb[target + 3];
                    for (unsigned c = 0; c < 3; ++c) {
                        if (thumb[target + c] > alpha)
                            throw std::runtime_error("Non-premultiplied render");
                        sheet[dest + c] =
                            static_cast<std::uint8_t>(thumb[target + c] + 32 * (255 - alpha) / 255);
                    }
                }
            renderer.writePng(output / L"thumbnails" / (p.id + ".png"), 128, 128, thumb);
            JsonObject item;
            item.Insert(L"id", JsonValue::CreateStringValue(to_hstring(p.id)));
            item.Insert(L"name", JsonValue::CreateStringValue(to_hstring(p.name)));
            item.Insert(L"family", JsonValue::CreateStringValue(to_hstring(p.family)));
            JsonArray tags;
            for (const auto& tag : p.tags)
                tags.Append(JsonValue::CreateStringValue(to_hstring(tag)));
            item.Insert(L"tags", tags);
            index.Append(item);
        }
        JsonObject manifest;
        manifest.Insert(L"schemaVersion", JsonValue::CreateNumberValue(1));
        manifest.Insert(L"presets", index);
        writeJsonFile(output / L"index.json", to_string(manifest.Stringify()));
        renderer.writePng(output / L"comparison.png", width, height, sheet);
        std::ofstream report(output / L"VALIDATION.md");
        report << "# Catalog validation\n\n"
               << presets.size() << " customizable types; legacy presets hidden from index; " << unique.size()
               << " distinct geometry fingerprints. Names, colors and layer order excluded.\n\nAll models "
                  "validated; JSON round trips and premultiplied alpha passed. Thumbnails and comparison.png "
                  "rendered by the production Direct2D/WIC renderer.\n\nComparison order: left-to-right, "
                  "top-to-bottom, matching index.json. Author/licensing assignment remains a release "
                  "decision; no external assets used.\n";
        std::cout << presets.size()
                  << " presets validated; no geometry duplicates; thumbnails and comparison generated.\n";
        std::cout << legacyPresets().size() + presets.size()
                  << " presets: independent geometry controls, save/reload and rendering verified.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (const hresult_error& e) {
        std::cerr << to_string(e.message()) << '\n';
        return 1;
    }
}
