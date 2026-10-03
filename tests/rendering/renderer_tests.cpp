#include "rendering/renderer.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

using namespace crosshair;
int main() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return 1;
    try {
        Renderer renderer;
        constexpr unsigned columns = 4, cell = 64, zoom = 4;
        const unsigned sheetWidth = columns * cell * zoom;
        const unsigned sheetHeight = static_cast<unsigned>(templatePresets().size()) * cell * zoom;
        std::vector<std::uint8_t> sheet(sheetWidth * sheetHeight * 4, 32);
        for (std::size_t i = 3; i < sheet.size(); i += 4) sheet[i] = 255;
        unsigned row = 0;
        for (const auto& source : templatePresets()) {
            unsigned column = 0;
            for (float thickness : {.5f, 2.f, 8.f, 16.f}) {
                auto preset = source;
                for (auto& l : preset.layers) {
                    l.thickness = thickness;
                    l.color = {1, 1, 1, 1};
                }
                const auto image = renderer.rasterize(preset);
                if (source.id == "type-corners" && thickness >= 8) {
                    const auto pixel = (139 * Surface::size + 139) * 4;
                    if (image.bgra[pixel] < 250 || image.bgra[pixel + 3] != 255)
                        throw std::runtime_error("Thick corner has a missing or outlined joint");
                }
                if (source.id == "type-chevron" && thickness >= 8) {
                    const auto pixel = (126 * Surface::size + 128) * 4;
                    if (image.bgra[pixel] < 250)
                        throw std::runtime_error("Chevron apex has a broken joint");
                }
                for (unsigned y = 0; y < cell * zoom; ++y)
                    for (unsigned x = 0; x < cell * zoom; ++x) {
                        const auto src = ((y / zoom + 96) * Surface::size + x / zoom + 96) * 4;
                        const auto dst = ((row * cell * zoom + y) * sheetWidth + column * cell * zoom + x) * 4;
                        for (unsigned c = 0; c < 3; ++c)
                            sheet[dst + c] = static_cast<std::uint8_t>(image.bgra[src + c] +
                                32 * (255 - image.bgra[src + 3]) / 255);
                    }
                for (auto& l : preset.layers) l.color.a = .5f;
                const auto translucent = renderer.rasterize(preset);
                for (std::size_t i = 0; i < translucent.bgra.size(); i += 4)
                    if (translucent.bgra[i + 3] > 128 || translucent.bgra[i] > translucent.bgra[i + 3])
                        throw std::runtime_error("Overlapping strokes accumulate opacity");
                std::reverse(preset.layers.begin(), preset.layers.end());
                for (auto& l : preset.layers)
                    if (l.shape == Shape::line) {
                        std::swap(l.x, l.endX);
                        std::swap(l.y, l.endY);
                    }
                const auto reversed = renderer.rasterize(preset);
                for (std::size_t i = 0; i < reversed.bgra.size(); ++i)
                    if (std::abs(int(reversed.bgra[i]) - int(translucent.bgra[i])) > 5)
                        throw std::runtime_error("Same-style geometry depends on layer order: " + source.id);
                ++column;
            }
            ++row;
        }
        renderer.writePng("stroke-comparison.png", sheetWidth, sheetHeight, sheet);
        std::cout << "8 shapes x 4 thicknesses: joints, layer order and opacity passed\n";
        for (const auto& preset : starterPresets()) {
            const auto image = renderer.rasterize(preset);
            for (std::size_t i = 0; i < image.bgra.size(); i += 4) {
                const auto alpha = image.bgra[i + 3];
                if (image.bgra[i] > alpha || image.bgra[i + 1] > alpha || image.bgra[i + 2] > alpha)
                    throw std::runtime_error("Non-premultiplied pixel");
            }
            if (image.bgra[3] != 0)
                throw std::runtime_error("Opaque background");
            const auto again = renderer.rasterize(preset);
            if (image.bgra != again.bgra)
                throw std::runtime_error("Non-deterministic rendering");
        }
        const auto dot = renderer.rasterize(starterPresets()[0]);
        const auto alphaAt = [&](unsigned x, unsigned y) {
            return dot.bgra[(y * Surface::size + x) * 4 + 3];
        };
        if (alphaAt(128, 128) != 255 || alphaAt(128, 120) != 0)
            throw std::runtime_error("Dot reference pixels differ");
        int maxDifference = 0;
        double totalAlpha = 0, weightedX = 0, weightedY = 0;
        for (unsigned y = 120; y <= 136; ++y)
            for (unsigned x = 120; x <= 136; ++x) {
                maxDifference =
                    std::max(maxDifference, std::abs(int(alphaAt(x, y)) - int(alphaAt(256 - x, 256 - y))));
                totalAlpha += alphaAt(x, y);
                weightedX += x * alphaAt(x, y);
                weightedY += y * alphaAt(x, y);
            }
        std::cout << "Alpha reflection max delta: " << maxDifference
                  << "; centroid: " << weightedX / totalAlpha << ',' << weightedY / totalAlpha << '\n';
        // Direct2D software AA produces up to 4/255 edge asymmetry on the pinned
        // SDK. Centroid drift is independently limited to 0.02 physical pixels.
        if (maxDifference > 4 || std::abs(weightedX / totalAlpha - 128) > 0.02 ||
            std::abs(weightedY / totalAlpha - 128) > 0.02)
            throw std::runtime_error("Geometric center drift");
        std::cout << "3 presets: deterministic premultiplied BGRA; dot reference and symmetry passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        CoUninitialize();
        return 1;
    }
    CoUninitialize();
}
