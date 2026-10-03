#include "apps/engine/storage.hpp"
#include "core/product.hpp"
#include <windows.h>
#include <shlobj.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace crosshair {
using namespace winrt::Windows::Data::Json;
constexpr std::size_t maxBytes = 65536;
void checkJsonLimits(std::string_view text, std::size_t maximumBytes) {
    if (text.empty() || text.size() > maximumBytes)
        throw std::runtime_error("JSON vuoto o oltre il limite di dimensione.");
    int depth = 0;
    bool quoted = false, escaped = false;
    for (const char c : text) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == '[' || c == '{') {
            if (++depth > 8)
                throw std::runtime_error("JSON troppo profondo.");
        } else if (c == ']' || c == '}')
            --depth;
    }
}
namespace {
std::filesystem::path testDataDirectory;
int number(const JsonObject& object, const wchar_t* key, int low, int high) {
    const double n = object.GetNamedNumber(key);
    if (!std::isfinite(n) || std::floor(n) != n || n < low || n > high)
        throw std::runtime_error("Valore numerico non valido.");
    return static_cast<int>(n);
}
void put(JsonObject const& o, const wchar_t* key, std::string_view value) {
    o.Insert(key, JsonValue::CreateStringValue(winrt::to_hstring(value)));
}
void put(JsonObject const& o, const wchar_t* key, int value) {
    o.Insert(key, JsonValue::CreateNumberValue(value));
}
void putBool(JsonObject const& o, const wchar_t* key, bool value) {
    o.Insert(key, JsonValue::CreateBooleanValue(value));
}
} // namespace
std::string readJsonFile(const std::filesystem::path& path, std::size_t maximumBytes) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Impossibile leggere il profilo.");
    const auto length = file.tellg();
    if (length < 0 || length > static_cast<std::streamoff>(maximumBytes))
        throw std::runtime_error("File oltre il limite di dimensione.");
    std::string text(static_cast<std::size_t>(length), '\0');
    file.seekg(0);
    if (!file.read(text.data(), length))
        throw std::runtime_error("Lettura del profilo incompleta.");
    return text;
}
std::filesystem::path dataDirectory() {
    if (!testDataDirectory.empty())
        return testDataDirectory;
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)))
        throw std::runtime_error("Cartella dati locale non disponibile.");
    const std::filesystem::path root(raw);
    CoTaskMemFree(raw);
    return root / product::directory;
}
void useTestDataDirectory(std::string_view token) {
    if (!validId(token))
        throw std::runtime_error("Token test non valido.");
    testDataDirectory =
        std::filesystem::temp_directory_path() / ("CrosshairNative.Test." + std::string(token));
    rejectReparsePath(testDataDirectory);
}
std::string encodeProfile(const Profile& p) {
    if (auto error = validate(p))
        throw std::runtime_error(*error);
    JsonObject root;
    put(root, L"schemaVersion", product::schemaVersion);
    put(root, L"id", p.id);
    put(root, L"name", p.name);
    putBool(root, L"enabled", p.enabled);
    put(root, L"backend", "standard");
    root.Insert(L"executablePath", JsonValue::CreateStringValue(p.executablePath));
    root.Insert(L"windowClass", JsonValue::CreateStringValue(p.windowClass));
    put(root, L"defaultPresetId", p.defaultPresetId);
    put(root, L"offsetX", p.offsetX);
    put(root, L"offsetY", p.offsetY);
    JsonArray slots;
    for (const auto& slot : p.slots) {
        JsonObject item;
        put(item, L"label", slot.label);
        put(item, L"presetId", slot.presetId);
        slots.Append(item);
    }
    root.Insert(L"slots", slots);
    JsonArray bindings;
    for (const auto& b : p.bindings) {
        JsonObject item;
        put(item, L"device", b.key.device == Device::keyboard ? "keyboard" : "mouse");
        put(item, L"code", b.key.code);
        put(item, L"extended", b.key.extended);
        put(item, L"requiredModifiers", b.requiredModifiers);
        putBool(item, L"allowExtraModifiers", b.allowExtraModifiers);
        put(item, L"action", b.action == Action::slot ? "slot" : b.action == Action::hide ? "hide" : "pause");
        put(item, L"slot", b.slot);
        bindings.Append(item);
    }
    root.Insert(L"bindings", bindings);
    return winrt::to_string(root.Stringify());
}
Profile decodeProfile(std::string_view text) {
    checkJsonLimits(text);
    try {
        const auto root = JsonObject::Parse(winrt::to_hstring(text));
        number(root, L"schemaVersion", product::schemaVersion, product::schemaVersion);
        if (root.GetNamedString(L"backend") != L"standard")
            throw std::runtime_error("Backend non supportato.");
        Profile p;
        p.id = winrt::to_string(root.GetNamedString(L"id"));
        p.name = winrt::to_string(root.GetNamedString(L"name"));
        p.enabled = root.GetNamedBoolean(L"enabled");
        p.executablePath = root.GetNamedString(L"executablePath");
        p.windowClass = root.GetNamedString(L"windowClass");
        p.defaultPresetId = winrt::to_string(root.GetNamedString(L"defaultPresetId"));
        p.offsetX = number(root, L"offsetX", -8192, 8192);
        p.offsetY = number(root, L"offsetY", -8192, 8192);
        const auto slots = root.GetNamedArray(L"slots");
        if (slots.Size() != 5)
            throw std::runtime_error("Sono richiesti cinque slot.");
        for (unsigned i = 0; i < 5; ++i) {
            const auto item = slots.GetObjectAt(i);
            p.slots[i] = {winrt::to_string(item.GetNamedString(L"label")),
                          winrt::to_string(item.GetNamedString(L"presetId"))};
        }
        const auto bindings = root.GetNamedArray(L"bindings");
        if (bindings.Size() > 7)
            throw std::runtime_error("Troppe associazioni.");
        for (const auto& value : bindings) {
            const auto item = value.GetObject();
            Binding b;
            const auto device = item.GetNamedString(L"device");
            if (device != L"keyboard" && device != L"mouse")
                throw std::runtime_error("Dispositivo non supportato.");
            b.key.device = device == L"keyboard" ? Device::keyboard : Device::mouse;
            b.key.code = static_cast<std::uint16_t>(number(item, L"code", 1, 127));
            b.key.extended = static_cast<std::uint8_t>(number(item, L"extended", 0, 2));
            b.requiredModifiers = static_cast<std::uint8_t>(number(item, L"requiredModifiers", 0, 15));
            b.allowExtraModifiers = item.GetNamedBoolean(L"allowExtraModifiers");
            const auto action = item.GetNamedString(L"action");
            if (action != L"slot" && action != L"hide" && action != L"pause")
                throw std::runtime_error("Azione non supportata.");
            b.action = action == L"slot" ? Action::slot : action == L"hide" ? Action::hide : Action::pause;
            b.slot = static_cast<std::uint8_t>(number(item, L"slot", 0, 4));
            p.bindings.push_back(b);
        }
        if (auto error = validate(p))
            throw std::runtime_error(*error);
        return p;
    } catch (const winrt::hresult_error&) {
        throw std::runtime_error("JSON non valido o schema non supportato.");
    }
}
LoadedProfile loadProfile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return {initialProfile(), {}, true};
    try {
        return {decodeProfile(readJsonFile(path)), {}, true};
    } catch (const std::exception&) {
        try {
            auto backup = path;
            backup += L".bak";
            auto p = decodeProfile(readJsonFile(backup));
            // Keep the corrupt original intact and forbid saves until explicit recovery.
            return {std::move(p),
                    "Profilo corrotto: caricato il backup in sola lettura. Usa Impostazioni → Ripristina "
                    "profilo recuperato per conservarlo come nuovo profilo attivo senza perdere l’originale.",
                    false};
        } catch (const std::exception&) {
            return {initialProfile(),
                    "Profilo e backup non leggibili. Dati conservati; configurazione temporanea non "
                    "salvabile. Usa Impostazioni → Ripristina profilo recuperato per iniziare di nuovo "
                    "conservando gli originali.",
                    false};
        }
    }
}
void saveProfile(const std::filesystem::path& path, const Profile& profile) {
    const auto text = encodeProfile(profile);
    writeJsonFile(path, text);
}
void writeJsonFile(const std::filesystem::path& path, std::string_view text, std::size_t maximumBytes) {
    checkJsonLimits(text, maximumBytes);
    rejectReparsePath(path);
    auto backup = path;
    backup += L".bak";
    rejectReparsePath(backup);
    std::filesystem::create_directories(path.parent_path());
    const auto attributes = GetFileAttributesW(path.parent_path().c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("La cartella profili non deve essere un collegamento.");
    auto temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
    HANDLE file =
        CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Impossibile creare il file temporaneo; controlla spazio e permessi.");
    DWORD written = 0;
    const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                    written == text.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("Salvataggio incompleto; configurazione precedente conservata.");
    }
    const bool replaced =
        std::filesystem::exists(path)
            ? ReplaceFileW(path.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr) != FALSE
            : MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!replaced) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error(
            "Sostituzione del profilo non riuscita; configurazione precedente conservata.");
    }
}
void rejectReparsePath(const std::filesystem::path& path) {
    for (auto parent = std::filesystem::absolute(path); !parent.empty();) {
        const auto attributes = GetFileAttributesW(parent.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Il percorso dati non deve attraversare collegamenti.");
        const auto next = parent.parent_path();
        if (next == parent)
            break;
        parent = next;
    }
}
std::filesystem::path profileFile(const std::filesystem::path& directory, std::string_view id) {
    if (!validId(id))
        throw std::runtime_error("ID profilo non valido.");
    return directory / ("profile-" + std::string(id) + ".json");
}
std::vector<Profile> savedProfiles(const std::filesystem::path& directory) {
    std::vector<Profile> result;
    if (!std::filesystem::exists(directory))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (result.size() >= 100)
            break;
        if (!entry.is_regular_file() || entry.path().extension() != L".json" ||
            !entry.path().filename().wstring().starts_with(L"profile-"))
            continue;
        const auto loaded = loadProfile(entry.path());
        if (loaded.writable &&
            entry.path().filename() == profileFile(directory, loaded.profile.id).filename())
            result.push_back(loaded.profile);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}
} // namespace crosshair
