#include "core/model.hpp"
#include "core/product.hpp"
#include "core/preset_json.hpp"
#include "apps/engine/storage.hpp"
#include "rendering/renderer.hpp"
#include "apps/ipc.hpp"
#include "apps/assets/resources.h"
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <sddl.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <psapi.h>
#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <map>
#include <fstream>

using namespace crosshair;
namespace {
std::wstring testToken;
constexpr UINT trayMessage = WM_APP + 1, refreshMessage = WM_APP + 2, openMessage = WM_APP + 3;
constexpr UINT exitCommand = 1, pauseCommand = 2, setupCommand = 3, resetCommand = 4;
constexpr UINT beginTestCommand = 5, endTestCommand = 6;
constexpr UINT profileTestCommand = 7;
constexpr UINT targetBase = 100, bindBase = 200, presetBase = 300, clearBase = 400;
constexpr UINT profileBase = 500;
struct Target {
    HWND window;
    std::wstring title, path, windowClass;
};

std::wstring wide(std::string_view value) {
    return winrt::to_hstring(value).c_str();
}
std::vector<std::string> preferenceIds(const winrt::Windows::Data::Json::JsonObject& object,
                                       const wchar_t* key, std::size_t limit) {
    std::vector<std::string> result;
    if (!object.HasKey(key))
        return result;
    const auto array = object.GetNamedArray(key);
    if (array.Size() > limit)
        throw std::runtime_error("Elenco preferenze oltre il limite.");
    for (auto const& value : array) {
        const auto id = winrt::to_string(value.GetString());
        if (!validId(id))
            throw std::runtime_error("ID preferenza non valido.");
        if (std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
    }
    return result;
}
winrt::Windows::Data::Json::JsonArray idArray(const std::vector<std::string>& ids) {
    winrt::Windows::Data::Json::JsonArray array;
    for (const auto& id : ids)
        array.Append(winrt::Windows::Data::Json::JsonValue::CreateStringValue(wide(id)));
    return array;
}
std::wstring processPath(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const bool ok = QueryFullProcessImageNameW(process, 0, path.data(), &length) != FALSE;
    CloseHandle(process);
    if (!ok)
        return {};
    path.resize(length);
    return path;
}
std::wstring windowClass(HWND window) {
    wchar_t buffer[256]{};
    GetClassNameW(window, buffer, 256);
    return buffer;
}
bool usable(HWND window) {
    if (!window || !IsWindowVisible(window) || IsIconic(window) || GetWindow(window, GW_OWNER))
        return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
        return false;
    return true;
}
std::uint8_t modifiers() {
    return ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? shift : 0) |
           ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ? control : 0) |
           ((GetAsyncKeyState(VK_MENU) & 0x8000) ? alt : 0) |
           (((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) ? windows : 0);
}
bool modifierKey(Key key) {
    return key.device == Device::keyboard &&
           (key.code == 0x2a || key.code == 0x36 || key.code == 0x1d || key.code == 0x38 ||
            (key.extended == 1 && (key.code == 0x5b || key.code == 0x5c)));
}
std::wstring keyName(const Binding& binding) {
    std::wstring name;
    if (binding.requiredModifiers & control)
        name += L"Ctrl + ";
    if (binding.requiredModifiers & shift)
        name += L"Shift + ";
    if (binding.requiredModifiers & alt)
        name += L"Alt + ";
    if (binding.key.device == Device::mouse)
        return name + L"Mouse " + std::to_wstring(binding.key.code);
    wchar_t buffer[128]{};
    LONG code = static_cast<LONG>(binding.key.code) << 16;
    if (binding.key.extended == 1)
        code |= 1 << 24;
    if (!GetKeyNameTextW(code, buffer, 128))
        return name + L"Scan " + std::to_wstring(binding.key.code);
    return name + buffer;
}

class Engine {
  public:
    HWND window = nullptr, overlay = nullptr, target = nullptr;
    bool refreshQueued = false;
    Engine() : renderer_(), path_(dataDirectory() / L"profiles" / L"active.json") {
        auto loaded = loadProfile(path_);
        profile_ = std::move(loaded.profile);
        warning_ = wide(loaded.warning);
        writable_ = loaded.writable;
        const auto preferences = dataDirectory() / L"settings.json";
        if (std::filesystem::exists(preferences))
            try {
                const auto text = readJsonFile(preferences);
                checkJsonLimits(text);
                const auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(text));
                const auto theme = json.GetNamedNumber(L"theme");
                if (json.GetNamedNumber(L"schemaVersion") != 1 || theme < 0 || theme > 2 ||
                    std::floor(theme) != theme)
                    throw std::runtime_error("Impostazioni non valide.");
                theme_ = static_cast<int>(theme);
                favorites_ = preferenceIds(json, L"favorites", 512);
                recents_ = preferenceIds(json, L"recents", 40);
                autoStart_ = json.GetNamedBoolean(L"autoStart", false);
                diagnostics_ = json.GetNamedBoolean(L"diagnostics", false);
            } catch (...) {
                preferencesWritable_ = false;
                theme_ = 0;
                favorites_.clear();
                recents_.clear();
                autoStart_ = false;
                diagnostics_ = false;
                warning_ += L" Impostazioni non leggibili: tema di sistema; file conservato per recupero.";
            }
        state_.profileEnabled = profile_.enabled;
        if (testToken.empty()) {
            wchar_t command[32768]{};
            DWORD size = sizeof(command);
            const auto expected =
                L"\"" + (installedDirectory() / L"CrosshairNative.Engine.exe").wstring() + L"\" --background";
            autoStart_ =
                RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                             L"CrosshairNative", RRF_RT_REG_SZ, nullptr, command, &size) == ERROR_SUCCESS &&
                expected == command;
        }
        state_.defaultPresetId = profile_.defaultPresetId;
        surfaces_ = prepareSurfaces(profile_);
        memoryDC_ = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = Surface::size;
        info.bmiHeader.biHeight = -static_cast<LONG>(Surface::size);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap_ = CreateDIBSection(memoryDC_, &info, DIB_RGB_COLORS, &pixels_, nullptr, 0);
        if (!memoryDC_ || !bitmap_) {
            if (bitmap_)
                DeleteObject(bitmap_);
            if (memoryDC_)
                DeleteDC(memoryDC_);
            throw std::runtime_error("Impossibile creare la superficie overlay.");
        }
        oldBitmap_ = SelectObject(memoryDC_, bitmap_);
    }
    ~Engine() {
        for (auto hook : hooks_)
            if (hook)
                UnhookWinEvent(hook);
        if (window)
            WTSUnRegisterSessionNotification(window);
        Shell_NotifyIconW(NIM_DELETE, &tray_);
        if (tray_.hIcon)
            DestroyIcon(tray_.hIcon);
        if (overlay)
            DestroyWindow(overlay);
        if (memoryDC_ && oldBitmap_)
            SelectObject(memoryDC_, oldBitmap_);
        if (bitmap_)
            DeleteObject(bitmap_);
        if (memoryDC_)
            DeleteDC(memoryDC_);
    }
    void initialize(bool quiet = false);
    void rawInput(HRAWINPUT input);
    void refresh();
    void menu();
    void command(UINT id);
    void dispatchIpc() {
        if (ipc_)
            ipc_->dispatch([this](auto text, DWORD caller) { return request(text, caller); });
    }
    void openSetup() {
        capture_.reset();
        wchar_t module[32768]{};
        if (GetModuleFileNameW(nullptr, module, 32768)) {
            const auto settings =
                std::filesystem::path(module).parent_path() / L"CrosshairNative.Settings.exe";
            if (std::filesystem::exists(settings)) {
                auto command = L"\"" + settings.wstring() + L"\"";
                STARTUPINFOW startup{};
                startup.cb = sizeof(startup);
                PROCESS_INFORMATION process{};
                if (CreateProcessW(settings.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                   nullptr, &startup, &process)) {
                    CloseHandle(process.hThread);
                    CloseHandle(process.hProcess);
                    return;
                }
                warning_ = L"Impostazioni non avviate. Ricompila la distribuzione WinUI completa.";
            }
        }
        SetWindowTextW(window, L"Crosshair Native — configurazione di sviluppo");
        ShowWindow(window, SW_SHOWNORMAL);
        SetForegroundWindow(window);
        InvalidateRect(window, nullptr, TRUE);
    }
    void cancelCapture() {
        capture_.reset();
        captureWindow_ = nullptr;
        captured_.reset();
        ShowWindow(window, SW_HIDE);
        refresh();
    }
    void paint() {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        RECT bounds{};
        GetClientRect(window, &bounds);
        InflateRect(&bounds, -20, -20);
        SetBkMode(dc, TRANSPARENT);
        std::wstring text =
            capture_ ? L"Premi il tasto da associare (oppure Mouse 3, 4 o 5).\nEsc annulla.\n\nI "
                       L"modificatori premuti diventano obbligatori; gli altri sono tollerati."
                     : L"Pannello tecnico del motore. La libreria completa e l’editor sono nelle "
                       L"impostazioni WinUI.\n\nUsa il "
                       L"menu dell’icona nell’area di notifica per:\n• scegliere una finestra bersaglio "
                       L"aperta;\n• registrare i tasti dei cinque slot, nascondi e pausa;\n• assegnare "
                       L"Punto, Croce aperta o Anello con punto.\n\nChiudi questa finestra per continuare a "
                       L"usare il motore.\nIl mirino segue il tasto dello slot, non il contenuto "
                       L"dell’inventario.\n\nSupporto finestra/borderless da verificare; nessuna garanzia "
                       L"anti-cheat.\nApri le impostazioni WinUI dal menu dell’icona tray.";
        if (!warning_.empty())
            text += L"\n\n" + warning_;
        DrawTextW(dc, text.c_str(), -1, &bounds, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        EndPaint(window, &ps);
    }
    void session(bool blocked) {
        sessionBlocked_ = blocked;
        refresh();
    }
    void stop() {
        state_.exiting = true;
        ShowWindow(overlay, SW_HIDE);
        PostQuitMessage(0);
    }

  private:
    Renderer renderer_;
    Profile profile_;
    State state_;
    InputEdges edges_;
    std::filesystem::path path_;
    std::map<std::string, Surface> surfaces_;
    std::vector<Target> targets_;
    std::vector<Profile> savedProfiles_;
    std::array<HWINEVENTHOOK, 5> hooks_{};
    NOTIFYICONDATAW tray_{};
    HDC memoryDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* pixels_ = nullptr;
    std::optional<Binding> capture_;
    std::optional<Binding> captured_;
    HWND captureWindow_ = nullptr;
    std::uint64_t captureId_ = 0, revision_ = 0, configRevision_ = 0;
    std::unique_ptr<ipc::Server> ipc_;
    std::wstring warning_;
    std::string drawnPreset_;
    POINT drawnPosition_{LONG_MIN, LONG_MIN};
    bool shown_ = false, sessionBlocked_ = false, writable_ = true;
    bool testMode_ = false;
    int theme_ = 0;
    std::vector<std::string> favorites_, recents_;
    bool autoStart_ = false;
    bool diagnostics_ = false;
    void diagnostic(const char* event) noexcept {
        if (!diagnostics_)
            return;
        try {
            const auto file = dataDirectory() / L"diagnostics" / L"engine.log";
            rejectReparsePath(file);
            std::filesystem::create_directories(file.parent_path());
            if (std::filesystem::exists(file) && std::filesystem::file_size(file) > 65536) {
                const auto backup = file.wstring() + L".1";
                rejectReparsePath(backup);
                if (!MoveFileExW(file.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING))
                    return;
            }
            SYSTEMTIME time{};
            GetSystemTime(&time);
            std::ofstream log(file, std::ios::app);
            log << time.wYear << '-' << time.wMonth << '-' << time.wDay << 'T' << time.wHour << ':'
                << time.wMinute << ':' << time.wSecond << "Z " << event << '\n';
        } catch (...) {
        }
    }
    bool preferencesWritable_ = true;
    void savePreferences(int theme, const std::vector<std::string>& favorites,
                         const std::vector<std::string>& recents, bool startup) {
        using namespace winrt::Windows::Data::Json;
        if (!preferencesWritable_)
            throw std::runtime_error(
                "Impostazioni in recupero: conserva il file e ripristina il backup prima di salvare.");
        JsonObject p;
        p.Insert(L"schemaVersion", JsonValue::CreateNumberValue(1));
        p.Insert(L"language", JsonValue::CreateStringValue(L"it-IT"));
        p.Insert(L"theme", JsonValue::CreateNumberValue(theme));
        p.Insert(L"favorites", idArray(favorites));
        p.Insert(L"recents", idArray(recents));
        p.Insert(L"autoStart", JsonValue::CreateBooleanValue(startup));
        p.Insert(L"diagnostics", JsonValue::CreateBooleanValue(diagnostics_));
        writeJsonFile(dataDirectory() / L"settings.json", winrt::to_string(p.Stringify()));
    }
    void input(Key key, bool down);
    std::map<std::string, Surface> prepareSurfaces(const Profile& p) {
        std::map<std::string, Surface> result;
        std::vector<std::string> ids{"original-dot", p.defaultPresetId};
        for (const auto& slot : p.slots)
            ids.push_back(slot.presetId);
        for (const auto& id : ids)
            if (!result.contains(id))
                try {
                    result.emplace(id, renderer_.rasterize(loadPreset(id)));
                } catch (const std::exception&) {
                    warning_ = L"Preset mancante o non valido: utilizzato il fallback. Riassegna il mirino "
                               L"nelle impostazioni.";
                }
        return result;
    }
    std::string request(std::string_view text, DWORD caller);
    void commit(Profile candidate) {
        if (!writable_)
            throw std::runtime_error(
                "Profilo in recupero: consulta l’avviso nella finestra di configurazione.");
        auto prepared = prepareSurfaces(candidate);
        if (!testMode_) {
            // Archive the old active profile before changing identity. Each file
            // is atomic; a partial failure preserves both recoverable snapshots.
            if (profile_.id != candidate.id)
                saveProfile(profileFile(path_.parent_path(), profile_.id), profile_);
            saveProfile(profileFile(path_.parent_path(), candidate.id), candidate);
            saveProfile(path_, candidate);
        }
        profile_ = std::move(candidate);
        surfaces_ = std::move(prepared);
        drawnPreset_.clear();
        ++configRevision_;
        state_.profileEnabled = profile_.enabled;
        state_.defaultPresetId = profile_.defaultPresetId;
        state_.selectedSlot.reset();
        state_.slotRequestsHidden = false;
        edges_.loseFocus();
        refresh();
    }
    void error(const std::exception& e) {
        warning_ = wide(e.what());
        openSetup();
    }
    void quarantineHeld() {
        for (const auto& b : profile_.bindings) {
            int vk = 0;
            if (b.key.device == Device::mouse)
                vk = b.key.code == 3 ? VK_MBUTTON : b.key.code == 4 ? VK_XBUTTON1 : VK_XBUTTON2;
            else
                vk = static_cast<int>(MapVirtualKeyExW(b.key.code | (b.key.extended == 1 ? 0xe000 : 0),
                                                       MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0)));
            if (vk && (GetAsyncKeyState(vk) & 0x8000))
                edges_.block(b.key);
        }
    }
};
Engine* engine = nullptr;
void CALLBACK onWindowEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG, DWORD, DWORD) {
    if (!engine || !engine->window)
        return;
    if ((event == EVENT_OBJECT_LOCATIONCHANGE || event == EVENT_OBJECT_DESTROY ||
         event == EVENT_OBJECT_HIDE) &&
        (hwnd != engine->target || object != OBJID_WINDOW))
        return;
    if (!engine->refreshQueued) {
        engine->refreshQueued = true;
        PostMessageW(engine->window, refreshMessage, 0, 0);
    }
}
LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_NCHITTEST)
        return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE)
        return MA_NOACTIVATE;
    return DefWindowProcW(window, message, w, l);
}
LRESULT CALLBACK engineProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (!engine)
        return DefWindowProcW(window, message, w, l);
    try {
        switch (message) {
        case WM_INPUT:
            engine->rawInput(reinterpret_cast<HRAWINPUT>(l));
            break;
        case ipc::dispatchMessage:
            engine->dispatchIpc();
            return 0;
        case ipc::shutdownMessage:
            engine->stop();
            return 0;
        case WM_PAINT:
            engine->paint();
            return 0;
        case WM_CLOSE:
            engine->cancelCapture();
            return 0;
        case WM_COMMAND:
            engine->command(LOWORD(w));
            return 0;
        case refreshMessage:
            engine->refreshQueued = false;
            engine->refresh();
            return 0;
        case openMessage:
            engine->openSetup();
            return 0;
        case trayMessage:
            if (LOWORD(l) == WM_CONTEXTMENU || LOWORD(l) == WM_RBUTTONUP)
                engine->menu();
            else if (LOWORD(l) == NIN_SELECT || LOWORD(l) == NIN_KEYSELECT)
                engine->openSetup();
            return 0;
        case WM_WTSSESSION_CHANGE:
            if (w == WTS_SESSION_LOCK || w == WTS_REMOTE_DISCONNECT || w == WTS_CONSOLE_DISCONNECT)
                engine->session(true);
            if (w == WTS_SESSION_UNLOCK || w == WTS_REMOTE_CONNECT || w == WTS_CONSOLE_CONNECT)
                engine->session(false);
            return 0;
        case WM_POWERBROADCAST:
            if (w == PBT_APMSUSPEND)
                engine->session(true);
            if (w == PBT_APMRESUMEAUTOMATIC)
                engine->session(false);
            return TRUE;
        case WM_DISPLAYCHANGE:
            engine->refresh();
            return 0;
        case WM_DPICHANGED: {
            const auto* r = reinterpret_cast<RECT*>(l);
            SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            engine->refresh();
            return 0;
        }
        case WM_ENDSESSION:
            if (w)
                engine->stop();
            return 0;
        }
    } catch (...) {
        engine->stop();
    }
    return DefWindowProcW(window, message, w, l); // Required cleanup for foreground WM_INPUT.
}
void Engine::initialize(bool quiet) {
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = engineProc;
    const auto engineClass = std::wstring(product::engineClass) + testToken;
    const auto overlayClass = std::wstring(product::overlayClass) + testToken;
    wc.lpszClassName = engineClass.c_str();
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    if (!RegisterClassW(&wc))
        throw std::runtime_error("Registrazione finestra non riuscita.");
    window = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, product::name.data(), WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 720, 460, nullptr, nullptr, instance, nullptr);
    if (!window)
        throw std::runtime_error("Creazione finestra non riuscita.");
    wc.lpfnWndProc = overlayProc;
    wc.lpszClassName = overlayClass.c_str();
    wc.hbrBackground = nullptr;
    if (!RegisterClassW(&wc))
        throw std::runtime_error("Registrazione overlay non riuscita.");
    overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
                                  WS_EX_TOPMOST,
                              wc.lpszClassName, L"", WS_POPUP, 0, 0, Surface::size, Surface::size, nullptr,
                              nullptr, instance, nullptr);
    if (!overlay)
        throw std::runtime_error("Creazione overlay non riuscita.");
    RAWINPUTDEVICE devices[] = {{0x01, 0x06, RIDEV_INPUTSINK, window}, {0x01, 0x02, RIDEV_INPUTSINK, window}};
    if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)))
        throw std::runtime_error("Raw Input non disponibile.");
    const DWORD flags = WINEVENT_OUTOFCONTEXT;
    hooks_[0] = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onWindowEvent, 0,
                                0, flags);
    hooks_[1] = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                                onWindowEvent, 0, 0, flags);
    hooks_[2] =
        SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, onWindowEvent, 0, 0, flags);
    hooks_[3] = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr, onWindowEvent, 0, 0, flags);
    hooks_[4] = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, onWindowEvent,
                                0, 0, flags);
    for (auto hook : hooks_)
        if (!hook)
            throw std::runtime_error("Osservazione finestre non disponibile.");
    if (!WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION))
        throw std::runtime_error("Notifiche sessione non disponibili.");
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = window;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = trayMessage;
    tray_.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_CROSSHAIR),
                                              IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                              GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (!tray_.hIcon)
        throw std::runtime_error("Icona dell'applicazione non disponibile.");
    wcscpy_s(tray_.szTip, L"Crosshair Native — In attesa del gioco");
    if (testToken.empty() && !Shell_NotifyIconW(NIM_ADD, &tray_))
        throw std::runtime_error("Area di notifica non disponibile.");
    tray_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &tray_);
    ipc_ = std::make_unique<ipc::Server>(window, testToken.empty() ? L"" : L".test." + testToken);
    diagnostic("engine_started");
    refresh();
    if (!quiet && (profile_.executablePath.empty() || !warning_.empty()))
        openSetup();
}
void Engine::refresh() {
    ++revision_;
    const HWND foreground = GetForegroundWindow();
    if (capture_ && captureWindow_ && foreground != captureWindow_) {
        capture_.reset();
        captureWindow_ = nullptr;
        captured_.reset();
    }
    HWND next = nullptr;
    if (!sessionBlocked_ && usable(foreground) && foreground != window && foreground != overlay &&
        !profile_.executablePath.empty()) {
        const auto path = processPath(foreground);
        if (!path.empty() && _wcsicmp(path.c_str(), profile_.executablePath.c_str()) == 0 &&
            windowClass(foreground) == profile_.windowClass)
            next = foreground;
    }
    if (next != target || state_.targetEligible != (next != nullptr)) {
        edges_.loseFocus();
        state_.setEligible(false);
        target = next;
        state_.setEligible(target != nullptr);
        if (target)
            quarantineHeld();
    }
    const auto selected = state_.visiblePreset(profile_);
    const wchar_t* status = state_.manualPaused         ? L"In pausa"
                            : !target                   ? L"In attesa del gioco"
                            : state_.slotRequestsHidden ? L"Nascosto per slot"
                            : !state_.selectedSlot      ? L"Preset predefinito"
                                                        : L"Attivo";
    const auto tooltip = std::wstring(product::name) + L" — " + status;
    if (tooltip != tray_.szTip) {
        wcsncpy_s(tray_.szTip, tooltip.c_str(), _TRUNCATE);
        tray_.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &tray_);
    }
    if (!selected || !target) {
        if (shown_)
            ShowWindow(overlay, SW_HIDE);
        shown_ = false;
        return;
    }
    RECT client{};
    if (!GetClientRect(target, &client) || client.right <= 0 || client.bottom <= 0) {
        ShowWindow(overlay, SW_HIDE);
        shown_ = false;
        return;
    }
    POINT center{client.right / 2 + profile_.offsetX, client.bottom / 2 + profile_.offsetY};
    if (!ClientToScreen(target, &center)) {
        ShowWindow(overlay, SW_HIDE);
        shown_ = false;
        return;
    }
    POINT destination{center.x - 128, center.y - 128};
    auto preset = surfaces_.find(*selected);
    if (preset == surfaces_.end()) {
        preset = surfaces_.find(profile_.defaultPresetId);
        if (preset == surfaces_.end())
            preset = surfaces_.find("original-dot");
        warning_ = L"Preset mancante: utilizzato il fallback. Riassegna il mirino dal menu Slot.";
    }
    if (preset == surfaces_.end()) {
        ShowWindow(overlay, SW_HIDE);
        shown_ = false;
        return;
    }
    if (drawnPreset_ != preset->first || drawnPosition_.x != destination.x ||
        drawnPosition_.y != destination.y) {
        if (drawnPreset_ != preset->first)
            memcpy(pixels_, preset->second.bgra.data(), preset->second.bgra.size());
        SIZE size{Surface::size, Surface::size};
        POINT source{};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(overlay, nullptr, &destination, &size, memoryDC_, &source, 0, &blend,
                                 ULW_ALPHA)) {
            state_.backendState = BackendState::unavailable;
            ShowWindow(overlay, SW_HIDE);
            shown_ = false;
            warning_ = L"Overlay non disponibile. Riavvia il motore e verifica la sessione desktop.";
            return;
        }
        drawnPreset_ = preset->first;
        drawnPosition_ = destination;
    }
    if (!shown_) {
        ShowWindow(overlay, SW_SHOWNOACTIVATE);
        shown_ = true;
    }
    SetWindowPos(overlay, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
void Engine::input(Key key, bool down) {
    if (!down) {
        edges_.release(key);
        return;
    }
    if (modifierKey(key))
        return;
    if (capture_ && GetForegroundWindow() == (captureWindow_ ? captureWindow_ : window)) {
        if (key.device == Device::keyboard && key.code == 1) {
            cancelCapture();
            return;
        }
        if (!edges_.press(key))
            return;
        auto candidate = profile_;
        auto binding = *capture_;
        binding.key = key;
        binding.requiredModifiers = modifiers();
        if (captureWindow_) {
            captured_ = binding;
            capture_.reset();
            ++captureId_;
            ++revision_;
            return;
        }
        std::erase_if(candidate.bindings, [&](const auto& b) {
            return b.action == binding.action && (b.action != Action::slot || b.slot == binding.slot);
        });
        candidate.bindings.push_back(binding);
        try {
            commit(std::move(candidate));
            capture_.reset();
            ShowWindow(window, SW_HIDE);
        } catch (const std::exception& e) {
            warning_ = wide(e.what());
            InvalidateRect(window, nullptr, TRUE);
        }
        return;
    }
    // Check foreground synchronously too: queued WinEvent callbacks must not
    // permit one last slot activation after another application takes focus.
    if (!target || GetForegroundWindow() != target || sessionBlocked_)
        return;
    if (std::none_of(profile_.bindings.begin(), profile_.bindings.end(),
                     [&](const auto& b) { return b.key == key; }))
        return;
    if (!edges_.press(key))
        return;
    if (const auto found = matchBinding(profile_.bindings, key, modifiers()))
        if (state_.apply(profile_.bindings[*found]))
            refresh();
}
void Engine::rawInput(HRAWINPUT handle) {
    RAWINPUT raw{};
    UINT size = sizeof(raw);
    if (GetRawInputData(handle, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == UINT(-1))
        return;
    if (raw.header.dwType == RIM_TYPEKEYBOARD) {
        const auto& key = raw.data.keyboard;
        if (key.VKey == 255 || key.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE)
            return;
        auto scan = key.MakeCode;
        if (!scan)
            scan =
                static_cast<USHORT>(MapVirtualKeyExW(key.VKey, MAPVK_VK_TO_VSC, GetKeyboardLayout(0)) & 0xff);
        if (!scan || scan > 127)
            return;
        input({Device::keyboard, scan,
               static_cast<std::uint8_t>((key.Flags & RI_KEY_E0)   ? 1
                                         : (key.Flags & RI_KEY_E1) ? 2
                                                                   : 0)},
              (key.Flags & RI_KEY_BREAK) == 0);
    } else if (raw.header.dwType == RIM_TYPEMOUSE) {
        const auto flags = raw.data.mouse.usButtonFlags;
        if (!flags)
            return; // No mouse movement storage or allocation, including high-rate mice.
        const std::array<USHORT, 3> down{RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_BUTTON_4_DOWN,
                                         RI_MOUSE_BUTTON_5_DOWN};
        const std::array<USHORT, 3> up{RI_MOUSE_MIDDLE_BUTTON_UP, RI_MOUSE_BUTTON_4_UP, RI_MOUSE_BUTTON_5_UP};
        for (unsigned i = 0; i < 3; ++i) {
            const Key key{Device::mouse, static_cast<std::uint16_t>(i + 3), 0};
            if (flags & up[i])
                input(key, false);
            if (flags & down[i])
                input(key, true);
        }
    }
}
void Engine::menu() {
    targets_.clear();
    EnumWindows(
        [](HWND window, LPARAM context) -> BOOL {
            auto& targets = *reinterpret_cast<std::vector<Target>*>(context);
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (!usable(window) || pid == GetCurrentProcessId() || targets.size() >= 80)
                return TRUE;
            wchar_t title[256]{};
            if (!GetWindowTextW(window, title, 256))
                return TRUE;
            auto path = processPath(window);
            if (path.empty())
                return TRUE;
            targets.push_back({window, title, std::move(path), windowClass(window)});
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&targets_));
    HMENU root = CreatePopupMenu(), targets = CreatePopupMenu();
    AppendMenuW(root, MF_STRING, setupCommand, L"Apri impostazioni");
    AppendMenuW(root, MF_STRING, pauseCommand,
                state_.manualPaused ? L"Riprendi (preset predefinito)" : L"Pausa");
    savedProfiles_ = savedProfiles(path_.parent_path());
    if (!savedProfiles_.empty()) {
        const auto profiles = CreatePopupMenu();
        for (std::size_t i = 0; i < savedProfiles_.size(); ++i)
            AppendMenuW(profiles, MF_STRING | (savedProfiles_[i].id == profile_.id ? MF_CHECKED : 0),
                        profileBase + i, wide(savedProfiles_[i].name).c_str());
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(profiles), L"Profilo attivo");
    }
    if (testMode_)
        AppendMenuW(root, MF_STRING, endTestCommand, L"Termina prova e ripristina profilo personale");
    for (std::size_t i = 0; i < targets_.size(); ++i)
        AppendMenuW(targets, MF_STRING, targetBase + i, targets_[i].title.c_str());
    if (targets_.empty())
        AppendMenuW(targets, MF_STRING | MF_GRAYED, 0, L"Nessuna finestra idonea — in attesa del gioco");
    AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(targets), L"Associa finestra bersaglio");
    for (unsigned i = 0; i < 7; ++i) {
        HMENU slotMenu = CreatePopupMenu();
        std::wstring label = i < 5    ? wide(profile_.slots[i].label)
                             : i == 5 ? L"Piccone / nascondi"
                                      : L"Pausa manuale";
        for (const auto& b : profile_.bindings)
            if ((i < 5 && b.action == Action::slot && b.slot == i) || (i == 5 && b.action == Action::hide) ||
                (i == 6 && b.action == Action::pause))
                label += L" — " + keyName(b);
        AppendMenuW(slotMenu, MF_STRING, bindBase + i, L"Registra tasto…");
        AppendMenuW(slotMenu, MF_STRING, clearBase + i, L"Rimuovi associazione");
        if (i < 5)
            for (unsigned j = 0; j < 3; ++j)
                AppendMenuW(slotMenu,
                            MF_STRING |
                                (profile_.slots[i].presetId == starterPresets()[j].id ? MF_CHECKED : 0),
                            presetBase + i * 3 + j, wide(starterPresets()[j].name).c_str());
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(slotMenu), label.c_str());
    }
    AppendMenuW(root, MF_STRING, resetCommand, L"Ripristina centro (offset 0, 0)");
    AppendMenuW(root, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(root, MF_STRING, exitCommand, L"Esci completamente");
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    const UINT choice =
        TrackPopupMenu(root, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, nullptr);
    DestroyMenu(root);
    PostMessageW(window, WM_NULL, 0, 0);
    if (choice)
        command(choice);
    refresh();
}
void Engine::command(UINT id) {
    try {
        if (id == exitCommand) {
            stop();
            return;
        }
        if (id == beginTestCommand || id == profileTestCommand) {
            // Explicit synthetic-target demo; never writes assumed keys into the
            // user's profile and cannot activate in a game window.
            wchar_t executable[32768]{};
            const auto length = GetModuleFileNameW(nullptr, executable, 32768);
            if (!length || length == 32768)
                throw std::runtime_error("Percorso del bersaglio di prova non disponibile.");
            auto demo = id == beginTestCommand ? initialProfile() : profile_;
            demo.name = "Prova temporanea";
            demo.executablePath =
                (std::filesystem::path(executable).parent_path() / L"CrosshairNative.TestTarget.exe")
                    .wstring();
            demo.windowClass = product::targetClass;
            if (id == beginTestCommand)
                for (std::uint8_t i = 0; i < 3; ++i)
                    demo.bindings.push_back(
                        {{Device::keyboard, static_cast<std::uint16_t>(i + 2), 0}, 0, true, Action::slot, i});
            surfaces_ = prepareSurfaces(demo);
            drawnPreset_.clear();
            profile_ = std::move(demo);
            state_ = State{};
            state_.profileEnabled = profile_.enabled;
            state_.defaultPresetId = profile_.defaultPresetId;
            edges_.loseFocus();
            testMode_ = true;
            capture_.reset();
            ShowWindow(window, SW_HIDE);
            refresh();
            return;
        }
        if (id == endTestCommand) {
            auto loaded = loadProfile(path_);
            profile_ = std::move(loaded.profile);
            writable_ = loaded.writable;
            warning_ = wide(loaded.warning);
            surfaces_ = prepareSurfaces(profile_);
            drawnPreset_.clear();
            state_ = State{};
            state_.profileEnabled = profile_.enabled;
            state_.defaultPresetId = profile_.defaultPresetId;
            testMode_ = false;
            edges_.loseFocus();
            refresh();
            return;
        }
        if (id == setupCommand) {
            openSetup();
            return;
        }
        if (id == pauseCommand) {
            state_.setPaused(!state_.manualPaused);
            refresh();
            return;
        }
        if (id >= bindBase && id < bindBase + 7) {
            captureWindow_ = nullptr;
            ShowWindow(window, SW_SHOWNORMAL);
            SetForegroundWindow(window);
            const unsigned i = id - bindBase;
            capture_ = Binding{{},
                               0,
                               true,
                               i < 5    ? Action::slot
                               : i == 5 ? Action::hide
                                        : Action::pause,
                               static_cast<std::uint8_t>(i < 5 ? i : 0)};
            edges_.loseFocus();
            warning_.clear();
            InvalidateRect(window, nullptr, TRUE);
            return;
        }
        auto candidate = profile_;
        if (id >= profileBase && id < profileBase + savedProfiles_.size()) {
            if (testMode_)
                throw std::runtime_error("Chiudi la prova prima di cambiare profilo.");
            candidate = savedProfiles_[id - profileBase];
        } else if (id >= targetBase && id < targetBase + targets_.size()) {
            const auto& selected = targets_[id - targetBase];
            candidate.executablePath = selected.path;
            candidate.windowClass = selected.windowClass;
        } else if (id >= presetBase && id < presetBase + 15) {
            candidate.slots[(id - presetBase) / 3].presetId = starterPresets()[(id - presetBase) % 3].id;
        } else if (id >= clearBase && id < clearBase + 7) {
            const unsigned i = id - clearBase;
            std::erase_if(candidate.bindings, [&](const auto& b) {
                return (i < 5 && b.action == Action::slot && b.slot == i) ||
                       (i == 5 && b.action == Action::hide) || (i == 6 && b.action == Action::pause);
            });
        } else if (id == resetCommand) {
            candidate.offsetX = 0;
            candidate.offsetY = 0;
        } else
            return;
        commit(std::move(candidate));
    } catch (const std::exception& e) {
        error(e);
    }
}
std::string Engine::request(std::string_view text, DWORD caller) {
    using namespace winrt::Windows::Data::Json;
    JsonObject reply;
    double requestId = 0;
    reply.Insert(L"version", JsonValue::CreateNumberValue(1));
    try {
        checkJsonLimits(text, ipc::maxPayload);
        const auto message = JsonObject::Parse(winrt::to_hstring(text));
        requestId = message.GetNamedNumber(L"id");
        if (requestId < 1 || requestId > 1000000000 || std::floor(requestId) != requestId ||
            message.GetNamedNumber(L"version") != 1)
            throw std::runtime_error("Versione o ID richiesta non valido.");
        const auto command = message.GetNamedString(L"command");
        const auto payload = message.GetNamedObject(L"payload");
        JsonObject result;
        if (command == L"GetState") {
            result.Insert(L"theme", JsonValue::CreateNumberValue(theme_));
            result.Insert(L"favorites", idArray(favorites_));
            result.Insert(L"recents", idArray(recents_));
            result.Insert(L"autoStart", JsonValue::CreateBooleanValue(autoStart_));
            result.Insert(L"diagnostics", JsonValue::CreateBooleanValue(diagnostics_));
            result.Insert(L"writable", JsonValue::CreateBooleanValue(writable_));
            result.Insert(L"profile", JsonObject::Parse(winrt::to_hstring(encodeProfile(profile_))));
            result.Insert(L"paused", JsonValue::CreateBooleanValue(state_.manualPaused));
            result.Insert(L"eligible", JsonValue::CreateBooleanValue(state_.targetEligible));
            result.Insert(L"visible", JsonValue::CreateBooleanValue(IsWindowVisible(overlay) != FALSE));
            result.Insert(L"backendAvailable",
                          JsonValue::CreateBooleanValue(state_.backendState == BackendState::ready));
            result.Insert(L"hiddenBySlot", JsonValue::CreateBooleanValue(state_.slotRequestsHidden));
            result.Insert(L"selectedSlot",
                          JsonValue::CreateNumberValue(state_.selectedSlot ? *state_.selectedSlot : -1));
            result.Insert(L"configRevision",
                          JsonValue::CreateNumberValue(static_cast<double>(configRevision_)));
            result.Insert(L"warning", JsonValue::CreateStringValue(warning_));
            result.Insert(L"testMode", JsonValue::CreateBooleanValue(testMode_));
            result.Insert(L"captureId", JsonValue::CreateNumberValue(static_cast<double>(captureId_)));
            result.Insert(L"capturing", JsonValue::CreateBooleanValue(capture_.has_value()));
            if (captured_) {
                const auto& b = *captured_;
                JsonObject binding;
                binding.Insert(L"device", JsonValue::CreateStringValue(
                                              b.key.device == Device::keyboard ? L"keyboard" : L"mouse"));
                binding.Insert(L"code", JsonValue::CreateNumberValue(b.key.code));
                binding.Insert(L"extended", JsonValue::CreateNumberValue(b.key.extended));
                binding.Insert(L"requiredModifiers", JsonValue::CreateNumberValue(b.requiredModifiers));
                binding.Insert(L"allowExtraModifiers", JsonValue::CreateBooleanValue(b.allowExtraModifiers));
                binding.Insert(L"action",
                               JsonValue::CreateStringValue(b.action == Action::slot   ? L"slot"
                                                            : b.action == Action::hide ? L"hide"
                                                                                       : L"pause"));
                binding.Insert(L"slot", JsonValue::CreateNumberValue(b.slot));
                binding.Insert(L"displayName", JsonValue::CreateStringValue(keyName(b)));
                result.Insert(L"captured", binding);
            }
        } else if (command == L"ListTargets") {
            JsonArray list;
            EnumWindows(
                [](HWND handle, LPARAM context) -> BOOL {
                    auto& array = *reinterpret_cast<JsonArray*>(context);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(handle, &pid);
                    if (!usable(handle) || pid == GetCurrentProcessId() || array.Size() >= 80)
                        return TRUE;
                    wchar_t title[256]{};
                    if (!GetWindowTextW(handle, title, 256))
                        return TRUE;
                    const auto path = processPath(handle);
                    if (path.empty())
                        return TRUE;
                    JsonObject target;
                    target.Insert(L"title", JsonValue::CreateStringValue(title));
                    target.Insert(L"executablePath", JsonValue::CreateStringValue(path));
                    target.Insert(L"windowClass", JsonValue::CreateStringValue(windowClass(handle)));
                    array.Append(target);
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&list));
            result.Insert(L"targets", list);
        } else if (command == L"RecoverProfile") {
            if (writable_ || testMode_ || !payload.GetNamedBoolean(L"confirmed", false) ||
                payload.GetNamedNumber(L"configRevision") != static_cast<double>(configRevision_))
                throw std::runtime_error("Recupero non disponibile. Ricarica lo stato.");
            auto preserved = path_;
            preserved += L".corrupt." + std::to_wstring(GetTickCount64());
            rejectReparsePath(path_);
            rejectReparsePath(preserved);
            if (!MoveFileExW(path_.c_str(), preserved.c_str(), MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Impossibile conservare l’originale: recupero annullato.");
            try {
                saveProfile(path_, profile_);
            } catch (...) {
                MoveFileExW(preserved.c_str(), path_.c_str(), MOVEFILE_WRITE_THROUGH);
                throw;
            }
            writable_ = true;
            warning_ =
                L"Profilo recuperato. L’originale non leggibile è conservato come active.json.corrupt.*.";
            ++configRevision_;
        } else if (command == L"ListProfiles") {
            JsonArray list;
            auto profiles = savedProfiles(path_.parent_path());
            std::erase_if(profiles, [this](const auto& p) { return p.id == profile_.id; });
            profiles.insert(profiles.begin(), profile_);
            for (const auto& p : profiles) {
                JsonObject item;
                item.Insert(L"id", JsonValue::CreateStringValue(wide(p.id)));
                item.Insert(L"name", JsonValue::CreateStringValue(wide(p.name)));
                list.Append(item);
            }
            result.Insert(L"profiles", list);
        } else if (command == L"SwitchProfile") {
            if (testMode_)
                throw std::runtime_error("Chiudi la prova prima di cambiare profilo.");
            if (payload.GetNamedNumber(L"configRevision") != static_cast<double>(configRevision_))
                throw std::runtime_error("Il profilo è cambiato. Ricarica prima di continuare.");
            const auto selected = winrt::to_string(payload.GetNamedString(L"profileId"));
            if (selected != profile_.id) {
                const auto file = profileFile(path_.parent_path(), selected);
                if (!std::filesystem::exists(file))
                    throw std::runtime_error("Profilo non trovato. Aggiorna l’elenco.");
                auto loaded = loadProfile(file);
                if (!loaded.writable)
                    throw std::runtime_error(loaded.warning);
                commit(std::move(loaded.profile));
            }
        } else if (command == L"ImportAsset") {
            const auto id = winrt::to_string(payload.GetNamedString(L"assetId"));
            if (!validId(id) || !id.starts_with("user-"))
                throw std::runtime_error("ID asset non valido.");
            const auto staged = dataDirectory() / L"staging" / ("asset-" + id + ".png");
            const auto destination = dataDirectory() / L"assets" / ("asset-" + id + ".png");
            rejectReparsePath(staged);
            rejectReparsePath(destination);
            renderer_.readPng(staged);
            std::filesystem::create_directories(destination.parent_path());
            if (!CopyFileW(staged.c_str(), destination.c_str(), TRUE))
                throw std::runtime_error("Asset non copiato. Controlla spazio e permessi.");
            DeleteFileW(staged.c_str());
        } else if (command == L"SavePreset" || command == L"UpdatePreset") {
            const auto p = decodePreset(winrt::to_string(payload.GetNamedObject(L"preset").Stringify()));
            if (!p.id.starts_with("user-"))
                throw std::runtime_error("I preset inclusi sono di sola lettura. Salva una copia personale.");
            auto surface = renderer_.rasterize(p);
            const auto path = dataDirectory() / L"presets" / ("preset-" + p.id + ".json");
            rejectReparsePath(path);
            const bool updating = command == L"UpdatePreset";
            if (updating && !std::filesystem::is_regular_file(path))
                throw std::runtime_error("Mirino personale non trovato. Salva una nuova copia.");
            if (!updating && std::filesystem::exists(path))
                throw std::runtime_error("ID preset già presente. Salva con un nuovo ID.");
            writeJsonFile(path, encodePreset(p));
            if (updating && surfaces_.contains(p.id)) {
                surfaces_.at(p.id) = std::move(surface);
                drawnPreset_.clear();
                refresh();
            }
        } else if (command == L"DeletePreset") {
            const auto id = winrt::to_string(payload.GetNamedString(L"presetId"));
            if (!validId(id) || !id.starts_with("user-"))
                throw std::runtime_error("Puoi eliminare soltanto i mirini personali.");
            if (!writable_ || testMode_)
                throw std::runtime_error("Chiudi la prova o recupera il profilo prima di eliminare un mirino.");
            const auto path = dataDirectory() / L"presets" / ("preset-" + id + ".json");
            rejectReparsePath(path);
            if (!std::filesystem::is_regular_file(path))
                throw std::runtime_error("Mirino personale non trovato.");
            const auto checkUsage = [&](const Profile& p) {
                if (p.defaultPresetId == id || std::any_of(p.slots.begin(), p.slots.end(),
                                                          [&](const Slot& slot) { return slot.presetId == id; }))
                    throw std::runtime_error("Mirino usato dal profilo \"" + p.name +
                                             "\". Sostituiscilo e salva il profilo prima di eliminarlo.");
            };
            checkUsage(profile_);
            // Read every saved profile strictly: a skipped or corrupt profile may still refer to this ID.
            if (std::filesystem::exists(path_.parent_path())) {
                for (const auto& entry : std::filesystem::directory_iterator(path_.parent_path())) {
                    const auto filename = entry.path().filename().wstring();
                    if (entry.path().extension() != L".json" ||
                        (filename != L"active.json" && !filename.starts_with(L"profile-")))
                        continue;
                    rejectReparsePath(entry.path());
                    checkUsage(decodeProfile(readJsonFile(entry.path())));
                }
            }
            auto favorites = favorites_, recents = recents_;
            std::erase(favorites, id);
            std::erase(recents, id);
            savePreferences(theme_, favorites, recents, autoStart_);
            try {
                if (!std::filesystem::remove(path))
                    throw std::runtime_error("Mirino non eliminato. Aggiorna la libreria e riprova.");
            } catch (...) {
                savePreferences(theme_, favorites_, recents_, autoStart_);
                throw;
            }
            favorites_ = std::move(favorites);
            recents_ = std::move(recents);
        } else if (command == L"SaveProfile") {
            if (payload.GetNamedNumber(L"configRevision") != static_cast<double>(configRevision_))
                throw std::runtime_error("Il profilo è cambiato. Aggiorna prima di salvare.");
            auto candidate = decodeProfile(winrt::to_string(payload.GetNamedObject(L"profile").Stringify()));
            if (testMode_)
                throw std::runtime_error("Chiudi la prova prima di salvare il profilo personale.");
            commit(std::move(candidate));
        } else if (command == L"SetTheme") {
            const auto theme = payload.GetNamedNumber(L"theme");
            if (theme < 0 || theme > 2 || std::floor(theme) != theme)
                throw std::runtime_error("Tema non valido.");
            savePreferences(static_cast<int>(theme), favorites_, recents_, autoStart_);
            theme_ = static_cast<int>(theme);
        } else if (command == L"SetFavorite" || command == L"MarkRecent") {
            const auto id = winrt::to_string(payload.GetNamedString(L"presetId"));
            loadPreset(id);
            auto favorites = favorites_, recents = recents_;
            if (command == L"SetFavorite") {
                std::erase(favorites, id);
                if (payload.GetNamedBoolean(L"favorite")) {
                    if (favorites.size() >= 512)
                        throw std::runtime_error("Massimo 512 preferiti.");
                    favorites.push_back(id);
                }
            } else {
                std::erase(recents, id);
                recents.insert(recents.begin(), id);
                if (recents.size() > 40)
                    recents.resize(40);
            }
            savePreferences(theme_, favorites, recents, autoStart_);
            favorites_ = std::move(favorites);
            recents_ = std::move(recents);
        } else if (command == L"SetDiagnostics") {
            const bool previous = diagnostics_;
            diagnostics_ = payload.GetNamedBoolean(L"enabled");
            try {
                savePreferences(theme_, favorites_, recents_, autoStart_);
            } catch (...) {
                diagnostics_ = previous;
                throw;
            }
            diagnostic("diagnostics_enabled");
        } else if (command == L"GetDiagnostics") {
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                 sizeof(memory));
            DWORD handles = 0;
            GetProcessHandleCount(GetCurrentProcess(), &handles);
            result.Insert(L"product", JsonValue::CreateStringValue(product::name));
            result.Insert(L"version", JsonValue::CreateStringValue(L"0.1.0 sviluppo locale"));
            result.Insert(L"architecture", JsonValue::CreateStringValue(L"x64"));
            result.Insert(L"backend", JsonValue::CreateStringValue(L"standard"));
            result.Insert(L"privateBytes",
                          JsonValue::CreateNumberValue(static_cast<double>(memory.PrivateUsage)));
            result.Insert(L"handles", JsonValue::CreateNumberValue(handles));
            result.Insert(L"cachedSurfaces",
                          JsonValue::CreateNumberValue(static_cast<double>(surfaces_.size())));
            result.Insert(L"visible", JsonValue::CreateBooleanValue(IsWindowVisible(overlay) != FALSE));
            result.Insert(L"targetEligible", JsonValue::CreateBooleanValue(state_.targetEligible));
        } else if (command == L"SetAutoStart") {
            if (!testToken.empty())
                throw std::runtime_error("Avvio automatico escluso dai test isolati.");
            const bool enable = payload.GetNamedBoolean(L"enabled");
            savePreferences(theme_, favorites_, recents_, enable);
            HKEY key = nullptr;
            LSTATUS error =
                RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                                nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
            if (error == ERROR_SUCCESS) {
                const auto startupCommand = L"\"" +
                                            (installedDirectory() / L"CrosshairNative.Engine.exe").wstring() +
                                            L"\" --background";
                error =
                    enable ? RegSetValueExW(key, product::directory.data(), 0, REG_SZ,
                                            reinterpret_cast<const BYTE*>(startupCommand.c_str()),
                                            static_cast<DWORD>((startupCommand.size() + 1) * sizeof(wchar_t)))
                           : RegDeleteValueW(key, product::directory.data());
                RegCloseKey(key);
                if (!enable && error == ERROR_FILE_NOT_FOUND)
                    error = ERROR_SUCCESS;
            }
            if (error != ERROR_SUCCESS) {
                savePreferences(theme_, favorites_, recents_, autoStart_);
                throw std::runtime_error(
                    "Avvio automatico non modificato. Verifica i permessi del tuo account.");
            }
            autoStart_ = enable;
        } else if (command == L"SetPaused") {
            state_.setPaused(payload.GetNamedBoolean(L"paused"));
            refresh();
        } else if (command == L"StartCapture") {
            const auto action = payload.GetNamedNumber(L"actionIndex");
            if (action < 0 || action > 6 || std::floor(action) != action)
                throw std::runtime_error("Azione non valida.");
            DWORD owner = 0;
            const auto foreground = GetForegroundWindow();
            GetWindowThreadProcessId(foreground, &owner);
            if (owner != caller)
                throw std::runtime_error("Porta le impostazioni in primo piano prima di registrare.");
            const auto i = static_cast<unsigned>(action);
            captureWindow_ = foreground;
            captured_.reset();
            capture_ = Binding{{},
                               0,
                               true,
                               i < 5    ? Action::slot
                               : i == 5 ? Action::hide
                                        : Action::pause,
                               static_cast<std::uint8_t>(i < 5 ? i : 0)};
            edges_.loseFocus();
            ++revision_;
        } else if (command == L"CancelCapture") {
            capture_.reset();
            captureWindow_ = nullptr;
            ++revision_;
        } else if (command == L"StartPreview") {
            auto preview = decodeProfile(winrt::to_string(payload.GetNamedObject(L"profile").Stringify()));
            profile_ = std::move(preview);
            this->command(profileTestCommand);
        } else if (command == L"EndTest") {
            this->command(endTestCommand);
        } else if (command == L"Shutdown") {
            ipc_->shutdownAfterReply();
        } else
            throw std::runtime_error("Comando IPC non supportato.");
        if (command == L"SaveProfile" || command == L"SwitchProfile") {
            result.Insert(L"configRevision",
                          JsonValue::CreateNumberValue(static_cast<double>(configRevision_)));
            result.Insert(L"profile", JsonObject::Parse(winrt::to_hstring(encodeProfile(profile_))));
        }
        reply.Insert(L"ok", JsonValue::CreateBooleanValue(true));
        reply.Insert(L"result", result);
        reply.Insert(L"errorCode", JsonValue::CreateStringValue(L""));
    } catch (const std::exception& e) {
        reply.Insert(L"ok", JsonValue::CreateBooleanValue(false));
        reply.Insert(L"errorCode", JsonValue::CreateStringValue(L"InvalidRequest"));
        reply.Insert(L"error", JsonValue::CreateStringValue(wide(e.what())));
    } catch (const winrt::hresult_error&) {
        reply.Insert(L"ok", JsonValue::CreateBooleanValue(false));
        reply.Insert(L"errorCode", JsonValue::CreateStringValue(L"MalformedMessage"));
        reply.Insert(L"error",
                     JsonValue::CreateStringValue(L"Messaggio non valido. Aggiorna lo stato e riprova."));
    }
    reply.Insert(L"id", JsonValue::CreateNumberValue(requestId));
    reply.Insert(L"revision", JsonValue::CreateNumberValue(static_cast<double>(revision_)));
    return winrt::to_string(reply.Stringify());
}
std::wstring instanceName() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        throw std::runtime_error("Identità utente non disponibile.");
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    std::vector<std::byte> data(bytes);
    const bool ok = GetTokenInformation(token, TokenUser, data.data(), bytes, &bytes) != FALSE;
    CloseHandle(token);
    LPWSTR sid = nullptr;
    if (!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid))
        throw std::runtime_error("SID non disponibile.");
    auto name = L"Local\\CrosshairNative.Engine." + std::wstring(sid) +
                (testToken.empty() ? L"" : L".test." + testToken);
    LocalFree(sid);
    return name;
}
} // namespace
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    HANDLE mutex = nullptr;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        if (std::wstring_view(arguments).starts_with(L"--self-test ")) {
            testToken = std::wstring_view(arguments).substr(12);
            useTestDataDirectory(winrt::to_string(testToken));
        }
        mutex = CreateMutexW(nullptr, FALSE, instanceName().c_str());
        if (!mutex)
            throw std::runtime_error("Controllo istanza non disponibile.");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (const auto existing = FindWindowW(product::engineClass.data(), nullptr)) {
                if (std::wstring_view(arguments) == L"--test")
                    PostMessageW(existing, WM_COMMAND, beginTestCommand, 0);
                else if (std::wstring_view(arguments) == L"--exit")
                    PostMessageW(existing, WM_COMMAND, exitCommand, 0);
                else if (std::wstring_view(arguments) != L"--background")
                    PostMessageW(existing, openMessage, 0, 0);
            }
            CloseHandle(mutex);
            return 0;
        }
        if (std::wstring_view(arguments) == L"--exit") {
            CloseHandle(mutex);
            return 0;
        }
        {
            Engine app;
            engine = &app;
            app.initialize(!testToken.empty() || std::wstring_view(arguments) == L"--background");
            if (std::wstring_view(arguments) == L"--test")
                app.command(beginTestCommand);
            MSG message{};
            int result = 0;
            while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (result == -1)
                throw std::runtime_error("Errore nella coda messaggi.");
            engine = nullptr;
        }
        CloseHandle(mutex);
        return 0;
    } catch (const std::exception& e) {
        engine = nullptr;
        if (mutex)
            CloseHandle(mutex);
        std::fprintf(stderr, "%s\n", e.what());
        if (testToken.empty())
            MessageBoxW(nullptr, wide(e.what()).c_str(), product::name.data(), MB_OK | MB_ICONERROR);
        return 1;
    } catch (...) {
        engine = nullptr;
        if (mutex)
            CloseHandle(mutex);
        if (testToken.empty())
            MessageBoxW(nullptr, L"Avvio non riuscito. Verifica i prerequisiti indicati in BUILD.md.",
                        product::name.data(), MB_OK | MB_ICONERROR);
        return 1;
    }
}
