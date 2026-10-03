#include "core/pack.hpp"
#include "core/preset_json.hpp"
#include "apps/engine/storage.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <set>
#include <algorithm>

namespace crosshair {
using namespace winrt;
using namespace winrt::Windows::Data::Json;
namespace {
std::string base64(std::span<const std::uint8_t> bytes) {
    DWORD length = 0;
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &length))
        throw std::runtime_error("Codifica asset non riuscita.");
    std::string encoded(length, '\0');
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &length))
        throw std::runtime_error("Codifica asset non riuscita.");
    encoded.resize(length);
    while (!encoded.empty() && encoded.back() == '\0')
        encoded.pop_back();
    return encoded;
}
std::vector<std::uint8_t> unbase64(std::string_view text) {
    DWORD length = 0;
    if (text.size() > 24 * 1024 * 1024 ||
        !CryptStringToBinaryA(text.data(), static_cast<DWORD>(text.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT, nullptr, &length, nullptr,
                              nullptr) ||
        length > 16 * 1024 * 1024)
        throw std::runtime_error("Asset Base64 non valido o oltre 16 MiB.");
    std::vector<std::uint8_t> bytes(length);
    if (!CryptStringToBinaryA(text.data(), static_cast<DWORD>(text.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT, bytes.data(), &length, nullptr,
                              nullptr))
        throw std::runtime_error("Asset Base64 non valido.");
    return bytes;
}
} // namespace
void validatePack(const Pack& p) {
    if (auto error = validate(p.profile))
        throw std::runtime_error(*error);
    if (p.presets.empty() || p.presets.size() + p.assets.size() + 1 > 2000)
        throw std::runtime_error("Pacchetto vuoto o oltre 2.000 elementi.");
    std::set<std::string> presets, assets;
    std::size_t bytes = 0, pixels = 0;
    for (const auto& a : p.assets) {
        if (!validId(a.id) || !assets.insert(a.id).second || a.png.size() < 33 ||
            a.png.size() > 16 * 1024 * 1024)
            throw std::runtime_error("Asset non valido o duplicato.");
        constexpr std::array<std::uint8_t, 16> header{137, 80, 78, 71, 13, 10, 26, 10,
                                                      0,   0,  0,  13, 73, 72, 68, 82};
        if (!std::equal(header.begin(), header.end(), a.png.begin()))
            throw std::runtime_error("È richiesto un PNG con intestazione valida.");
        auto integer = [&](int start) {
            std::uint32_t value = 0;
            for (int i = 0; i < 4; ++i)
                value = (value << 8) | a.png[start + i];
            return value;
        };
        const auto width = integer(16), height = integer(20);
        if (!width || !height || width > 2048 || height > 2048)
            throw std::runtime_error("Asset oltre 2048 × 2048 pixel.");
        pixels += static_cast<std::size_t>(width) * height * 4;
        bytes += a.png.size();
        if (bytes > maxPackBytes || pixels > maxPackBytes)
            throw std::runtime_error("Pacchetto oltre 100 MiB espansi.");
    }
    for (const auto& preset : p.presets) {
        if (auto error = validate(preset))
            throw std::runtime_error(*error);
        if (!presets.insert(preset.id).second)
            throw std::runtime_error("ID preset duplicato.");
        for (const auto& l : preset.layers)
            if (l.shape == Shape::png && !assets.contains(l.assetId))
                throw std::runtime_error("Asset PNG mancante.");
    }
    if (!presets.contains(p.profile.defaultPresetId))
        throw std::runtime_error("Fallback mancante nel pacchetto.");
    for (const auto& slot : p.profile.slots)
        if (!presets.contains(slot.presetId))
            throw std::runtime_error("Preset di uno slot mancante.");
}
std::string encodePack(const Pack& p) {
    validatePack(p);
    JsonObject root, manifest;
    manifest.Insert(L"schemaVersion", JsonValue::CreateNumberValue(1));
    manifest.Insert(L"format", JsonValue::CreateStringValue(L"CrosshairNativePack"));
    root.Insert(L"manifest", manifest);
    root.Insert(L"profile", JsonObject::Parse(to_hstring(encodeProfile(p.profile))));
    JsonArray presets, assets;
    for (const auto& preset : p.presets)
        presets.Append(JsonObject::Parse(to_hstring(encodePreset(preset))));
    for (const auto& asset : p.assets) {
        JsonObject a;
        a.Insert(L"id", JsonValue::CreateStringValue(to_hstring(asset.id)));
        a.Insert(L"pngBase64", JsonValue::CreateStringValue(to_hstring(base64(asset.png))));
        assets.Append(a);
    }
    root.Insert(L"presets", presets);
    root.Insert(L"assets", assets);
    const auto text = to_string(root.Stringify());
    checkJsonLimits(text, maxPackBytes);
    return text;
}
Pack decodePack(std::string_view text) {
    checkJsonLimits(text, maxPackBytes);
    try {
        const auto root = JsonObject::Parse(to_hstring(text));
        const auto manifest = root.GetNamedObject(L"manifest");
        if (manifest.GetNamedNumber(L"schemaVersion") != 1 ||
            manifest.GetNamedString(L"format") != L"CrosshairNativePack")
            throw std::runtime_error("Formato pacchetto non supportato.");
        Pack p;
        p.profile = decodeProfile(to_string(root.GetNamedObject(L"profile").Stringify()));
        const auto presets = root.GetNamedArray(L"presets"), assets = root.GetNamedArray(L"assets");
        if (presets.Size() + assets.Size() + 1 > 2000)
            throw std::runtime_error("Pacchetto oltre 2.000 elementi.");
        for (auto const& value : presets)
            p.presets.push_back(decodePreset(to_string(value.GetObject().Stringify())));
        std::size_t total = 0;
        for (auto const& value : assets) {
            const auto a = value.GetObject();
            auto bytes = unbase64(to_string(a.GetNamedString(L"pngBase64")));
            total += bytes.size();
            if (total > maxPackBytes)
                throw std::runtime_error("Pacchetto oltre il limite.");
            p.assets.push_back({to_string(a.GetNamedString(L"id")), std::move(bytes)});
        }
        validatePack(p);
        return p;
    } catch (const hresult_error&) {
        throw std::runtime_error("Pacchetto JSON non valido.");
    }
}
} // namespace crosshair
