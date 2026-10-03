#include "core/preset_json.hpp"
#include "apps/engine/storage.hpp"
#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <cmath>

namespace crosshair {
using namespace winrt;
using namespace Windows::Data::Json;
namespace {
void string(JsonObject const& o, const wchar_t* key, std::string_view value) {
    o.Insert(key, JsonValue::CreateStringValue(to_hstring(value)));
}
void number(JsonObject const& o, const wchar_t* key, double value) {
    o.Insert(key, JsonValue::CreateNumberValue(value));
}
float real(JsonObject const& o, const wchar_t* key) {
    const double n = o.GetNamedNumber(key);
    if (!std::isfinite(n) || std::abs(n) > 65536)
        throw std::runtime_error("Parametro preset non valido.");
    return static_cast<float>(n);
}
} // namespace
std::string encodePreset(const Preset& p) {
    if (auto e = validate(p))
        throw std::runtime_error(*e);
    JsonObject o;
    number(o, L"schemaVersion", 1);
    string(o, L"id", p.id);
    string(o, L"name", p.name);
    string(o, L"family", p.family);
    JsonObject provenance;
    string(provenance, L"origin", p.provenance);
    string(provenance, L"author", p.author);
    string(provenance, L"source", p.source);
    string(provenance, L"license", p.license);
    string(provenance, L"permission", p.permission);
    o.Insert(L"provenance", provenance);
    JsonArray tags;
    for (const auto& tag : p.tags)
        tags.Append(JsonValue::CreateStringValue(to_hstring(tag)));
    o.Insert(L"tags", tags);
    JsonArray layers;
    for (const auto& l : p.layers) {
        JsonObject a;
        string(a, L"shape",
               l.shape == Shape::dot    ? "dot"
               : l.shape == Shape::ring ? "ring"
               : l.shape == Shape::png  ? "png"
                                        : "line");
        number(a, L"x", l.x);
        number(a, L"y", l.y);
        number(a, L"endX", l.endX);
        number(a, L"endY", l.endY);
        number(a, L"radius", l.radius);
        number(a, L"thickness", l.thickness);
        number(a, L"outline", l.outline);
        number(a, L"rotation", l.rotation);
        if (l.shape == Shape::png) {
            string(a, L"assetId", l.assetId);
            number(a, L"width", l.width);
            number(a, L"height", l.height);
            a.Insert(L"nearest", JsonValue::CreateBooleanValue(l.nearest));
        }
        JsonArray color;
        for (const auto v : {l.color.r, l.color.g, l.color.b, l.color.a})
            color.Append(JsonValue::CreateNumberValue(v));
        a.Insert(L"color", color);
        layers.Append(a);
    }
    o.Insert(L"layers", layers);
    return to_string(o.Stringify());
}
Preset decodePreset(std::string_view text) {
    checkJsonLimits(text);
    try {
        const auto o = JsonObject::Parse(to_hstring(text));
        if (o.GetNamedNumber(L"schemaVersion") != 1)
            throw std::runtime_error("Versione preset non supportata.");
        Preset p;
        p.id = to_string(o.GetNamedString(L"id"));
        p.name = to_string(o.GetNamedString(L"name"));
        p.family = to_string(o.GetNamedString(L"family", L"personali"));
        if (o.HasKey(L"provenance")) {
            const auto provenance = o.GetNamedObject(L"provenance");
            p.provenance = to_string(provenance.GetNamedString(L"origin", L"non specificata"));
            p.author = to_string(provenance.GetNamedString(L"author", L""));
            p.source = to_string(provenance.GetNamedString(L"source", L""));
            p.license = to_string(provenance.GetNamedString(L"license", L""));
            p.permission = to_string(provenance.GetNamedString(L"permission", L""));
        }
        if (p.family.size() > 60)
            throw std::runtime_error("Famiglia troppo lunga.");
        if (o.HasKey(L"tags")) {
            auto tags = o.GetNamedArray(L"tags");
            if (tags.Size() > 16)
                throw std::runtime_error("Troppi tag.");
            for (auto const& t : tags) {
                auto tag = to_string(t.GetString());
                if (tag.size() > 60)
                    throw std::runtime_error("Tag troppo lungo.");
                p.tags.push_back(tag);
            }
        }
        const auto layers = o.GetNamedArray(L"layers");
        if (layers.Size() > 16)
            throw std::runtime_error("Massimo 16 livelli.");
        for (auto const& value : layers) {
            const auto a = value.GetObject();
            Layer l;
            const auto shape = a.GetNamedString(L"shape");
            if (shape != L"dot" && shape != L"line" && shape != L"ring" && shape != L"png")
                throw std::runtime_error("Primitiva non supportata.");
            l.shape = shape == L"dot"    ? Shape::dot
                      : shape == L"line" ? Shape::line
                      : shape == L"png"  ? Shape::png
                                         : Shape::ring;
            l.x = real(a, L"x");
            l.y = real(a, L"y");
            l.endX = real(a, L"endX");
            l.endY = real(a, L"endY");
            l.radius = real(a, L"radius");
            l.thickness = real(a, L"thickness");
            l.outline = real(a, L"outline");
            if (a.HasKey(L"rotation"))
                l.rotation = real(a, L"rotation");
            if (l.shape == Shape::png) {
                l.assetId = to_string(a.GetNamedString(L"assetId"));
                l.width = real(a, L"width");
                l.height = real(a, L"height");
                l.nearest = a.GetNamedBoolean(L"nearest");
            }
            const auto c = a.GetNamedArray(L"color");
            if (c.Size() != 4)
                throw std::runtime_error("Colore non valido.");
            l.color = {static_cast<float>(c.GetNumberAt(0)), static_cast<float>(c.GetNumberAt(1)),
                       static_cast<float>(c.GetNumberAt(2)), static_cast<float>(c.GetNumberAt(3))};
            p.layers.push_back(l);
        }
        if (auto e = validate(p))
            throw std::runtime_error(*e);
        return p;
    } catch (const hresult_error&) {
        throw std::runtime_error("Preset JSON non valido.");
    }
}
std::filesystem::path installedDirectory() {
    wchar_t file[32768]{};
    if (!GetModuleFileNameW(nullptr, file, 32768))
        throw std::runtime_error("Cartella installazione non disponibile.");
    return std::filesystem::path(file).parent_path();
}
Preset loadPreset(std::string_view id) {
    if (!validId(id))
        throw std::runtime_error("ID preset non valido.");
    if (const auto* p = findPreset(id))
        return *p;
    const auto name = "preset-" + std::string(id) + ".json";
    const auto personal = dataDirectory() / L"presets" / name;
    const auto path =
        std::filesystem::exists(personal) ? personal : installedDirectory() / L"catalog" / L"presets" / name;
    auto p = decodePreset(readJsonFile(path));
    if (p.id != id)
        throw std::runtime_error("ID preset differente dal file.");
    return p;
}
} // namespace crosshair
