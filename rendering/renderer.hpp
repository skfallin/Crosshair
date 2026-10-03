#pragma once
#include "core/model.hpp"
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>

namespace crosshair {
struct Surface {
    static constexpr unsigned size = 257;
    std::vector<std::uint8_t> bgra;
};
struct PngImage {
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> bgra;
};
class Renderer {
  public:
    Renderer();
    Surface rasterize(const Preset& preset) const;
    void writePng(const std::filesystem::path& path, unsigned width, unsigned height,
                  std::span<const std::uint8_t> bgra) const;
    PngImage readPng(const std::filesystem::path& path) const;

  private:
    Microsoft::WRL::ComPtr<ID2D1Factory> d2d_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_;
};
} // namespace crosshair
