#include "rendering/renderer.hpp"
#include "apps/engine/storage.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <deque>

namespace crosshair {
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("Rasterizzazione Direct2D/WIC non riuscita.");
}
bool sameStroke(const Layer& a, const Layer& b) {
    return b.shape != Shape::png && a.rotation == b.rotation && a.thickness == b.thickness &&
           a.outline == b.outline && a.color.r == b.color.r && a.color.g == b.color.g &&
           a.color.b == b.color.b && a.color.a == b.color.a;
}
ComPtr<ID2D1PathGeometry> joinedLines(ID2D1Factory* factory, std::span<const Layer> layers) {
    ComPtr<ID2D1PathGeometry> path;
    checked(factory->CreatePathGeometry(&path));
    ComPtr<ID2D1GeometrySink> sink;
    checked(path->Open(&sink));
    const auto point = [](const Layer& l, bool end) {
        return D2D1::Point2F(end ? l.endX : l.x, end ? l.endY : l.y);
    };
    const auto equal = [](D2D1_POINT_2F a, D2D1_POINT_2F b) { return a.x == b.x && a.y == b.y; };
    const auto degree = [&](D2D1_POINT_2F p) {
        int count = 0;
        for (const auto& l : layers)
            if (l.shape == Shape::line)
                count += equal(p, point(l, false)) + equal(p, point(l, true));
        return count;
    };
    std::vector<bool> used(layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (used[i] || layers[i].shape != Shape::line)
            continue;
        used[i] = true;
        std::deque<D2D1_POINT_2F> points{point(layers[i], false), point(layers[i], true)};
        // Join only genuine shared endpoints, leaving gaps and branching arms intact.
        for (bool front : {false, true}) {
            while (!equal(points.front(), points.back()) && degree(front ? points.front() : points.back()) == 2) {
                const auto p = front ? points.front() : points.back();
                bool extended = false;
                for (std::size_t j = 0; j < layers.size(); ++j) {
                    if (used[j] || layers[j].shape != Shape::line)
                        continue;
                    const auto a = point(layers[j], false), b = point(layers[j], true);
                    if (!equal(p, a) && !equal(p, b))
                        continue;
                    const auto next = equal(p, a) ? b : a;
                    if (front) points.push_front(next);
                    else points.push_back(next);
                    used[j] = extended = true;
                    break;
                }
                if (!extended)
                    break;
            }
        }
        const bool closed = points.size() > 2 && equal(points.front(), points.back());
        if (closed)
            points.pop_back();
        sink->BeginFigure(points.front(), D2D1_FIGURE_BEGIN_HOLLOW);
        for (std::size_t j = 1; j < points.size(); ++j)
            sink->AddLine(points[j]);
        sink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    }
    checked(sink->Close());
    return path;
}
} // namespace
Renderer::Renderer() {
    checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.GetAddressOf()));
    checked(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(wic_.GetAddressOf())));
}
Surface Renderer::rasterize(const Preset& preset) const {
    if (auto error = validate(preset))
        throw std::runtime_error(*error);
    ComPtr<IWICBitmap> bitmap;
    checked(wic_->CreateBitmap(Surface::size, Surface::size, GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapCacheOnLoad, &bitmap));
    ComPtr<ID2D1RenderTarget> target;
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    checked(d2d_->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target));
    ComPtr<ID2D1SolidColorBrush> brush;
    checked(target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush));
    ComPtr<ID2D1StrokeStyle> stroke;
    auto strokeProperties = D2D1::StrokeStyleProperties();
    strokeProperties.lineJoin = D2D1_LINE_JOIN_MITER_OR_BEVEL;
    strokeProperties.miterLimit = 2;
    checked(d2d_->CreateStrokeStyle(strokeProperties, nullptr, 0, &stroke));
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    // Odd surface size puts the logical origin on the central pixel's center.
    target->SetTransform(D2D1::Matrix3x2F::Translation(128.5f, 128.5f));
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for (std::size_t first = 0; first < preset.layers.size();) {
        const auto& l = preset.layers[first];
        target->SetTransform(D2D1::Matrix3x2F::Rotation(l.rotation) *
                             D2D1::Matrix3x2F::Translation(128.5f, 128.5f));
        if (l.shape == Shape::png) {
            const auto decoded = readPng(dataDirectory() / L"assets" / ("asset-" + l.assetId + ".png"));
            ComPtr<ID2D1Bitmap> image;
            const auto format = D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
            checked(target->CreateBitmap(D2D1::SizeU(decoded.width, decoded.height), decoded.bgra.data(),
                                         decoded.width * 4, format, &image));
            target->DrawBitmap(
                image.Get(),
                D2D1::RectF(l.x - l.width / 2, l.y - l.height / 2, l.x + l.width / 2, l.y + l.height / 2),
                l.color.a,
                l.nearest ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                          : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            ++first;
            continue;
        }
        auto end = first + 1;
        while (end < preset.layers.size() && sameStroke(l, preset.layers[end]))
            ++end;
        const auto layers = std::span(preset.layers).subspan(first, end - first);
        const auto lines = joinedLines(d2d_.Get(), layers);
        // Paint a same-style run as one object: outlines behind all fills, opacity once.
        const bool transparent = l.color.a < 1;
        if (transparent)
            target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), l.color.a), nullptr);
        const auto draw = [&](bool outline) {
            const auto extra = outline ? l.outline : 0.0f;
            brush->SetColor(outline ? D2D1::ColorF(0, 0, 0, 1)
                                    : D2D1::ColorF(l.color.r, l.color.g, l.color.b, 1));
            target->DrawGeometry(lines.Get(), brush.Get(), l.thickness + 2 * extra, stroke.Get());
            for (const auto& shape : layers) {
                if (shape.shape == Shape::dot)
                    target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(shape.x, shape.y),
                        shape.radius + extra, shape.radius + extra), brush.Get());
                else if (shape.shape == Shape::ring)
                    target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(shape.x, shape.y),
                        shape.radius, shape.radius), brush.Get(), shape.thickness + 2 * extra);
            }
        };
        if (l.outline > 0)
            draw(true);
        draw(false);
        if (transparent)
            target->PopLayer();
        first = end;
    }
    checked(target->EndDraw());
    Surface result;
    result.bgra.resize(Surface::size * Surface::size * 4);
    checked(bitmap->CopyPixels(nullptr, Surface::size * 4, static_cast<UINT>(result.bgra.size()),
                               result.bgra.data()));
    return result;
}
PngImage Renderer::readPng(const std::filesystem::path& path) const {
    rejectReparsePath(path);
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        throw std::runtime_error("PNG non leggibile.");
    const auto size = input.tellg();
    if (size <= 0 || size > 16 * 1024 * 1024)
        throw std::runtime_error("PNG oltre 16 MiB.");
    std::vector<BYTE> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), size))
        throw std::runtime_error("PNG incompleto.");
    ComPtr<IWICStream> stream;
    checked(wic_->CreateStream(&stream));
    checked(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;
    checked(wic_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder));
    GUID format{};
    checked(decoder->GetContainerFormat(&format));
    if (format != GUID_ContainerFormatPng)
        throw std::runtime_error("È richiesto un PNG.");
    UINT frames = 0;
    checked(decoder->GetFrameCount(&frames));
    if (frames != 1)
        throw std::runtime_error("Sono ammesse solo immagini statiche.");
    ComPtr<IWICBitmapFrameDecode> frame;
    checked(decoder->GetFrame(0, &frame));
    PngImage result;
    checked(frame->GetSize(&result.width, &result.height));
    if (!result.width || !result.height || result.width > 2048 || result.height > 2048)
        throw std::runtime_error("PNG oltre 2048 × 2048 pixel.");
    ComPtr<IWICFormatConverter> converter;
    checked(wic_->CreateFormatConverter(&converter));
    checked(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                  nullptr, 0, WICBitmapPaletteTypeCustom));
    result.bgra.resize(static_cast<std::size_t>(result.width) * result.height * 4);
    checked(converter->CopyPixels(nullptr, result.width * 4, static_cast<UINT>(result.bgra.size()),
                                  result.bgra.data()));
    return result;
}
void Renderer::writePng(const std::filesystem::path& path, unsigned width, unsigned height,
                        std::span<const std::uint8_t> pixels) const {
    if (!width || !height || width > 4096 || height > 4096 ||
        pixels.size() != static_cast<std::size_t>(width) * height * 4)
        throw std::runtime_error("Dimensioni PNG non valide.");
    ComPtr<IWICBitmap> bitmap;
    checked(wic_->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppPBGRA, width * 4,
                                         static_cast<UINT>(pixels.size()), const_cast<BYTE*>(pixels.data()),
                                         &bitmap));
    ComPtr<IWICFormatConverter> straight;
    checked(wic_->CreateFormatConverter(&straight));
    checked(straight->Initialize(bitmap.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr,
                                 0, WICBitmapPaletteTypeCustom));
    ComPtr<IWICStream> stream;
    checked(wic_->CreateStream(&stream));
    checked(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;
    checked(wic_->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
    checked(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    checked(encoder->CreateNewFrame(&frame, nullptr));
    checked(frame->Initialize(nullptr));
    checked(frame->SetSize(width, height));
    auto format = GUID_WICPixelFormat32bppBGRA;
    checked(frame->SetPixelFormat(&format));
    checked(frame->WriteSource(straight.Get(), nullptr));
    checked(frame->Commit());
    checked(encoder->Commit());
}
} // namespace crosshair
