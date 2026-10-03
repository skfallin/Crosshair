#include "apps/ipc.hpp"
#include "apps/engine/storage.hpp"
#include "core/preset_json.hpp"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <windows.h>
#include <objbase.h>
#include <psapi.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace crosshair;
using namespace winrt;
using namespace winrt::Windows::Data::Json;
struct Child {
    HANDLE process = nullptr;
    ~Child() {
        if (process) {
            if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
                TerminateProcess(process, 1);
            CloseHandle(process);
        }
    }
    void start(std::wstring_view token) {
        const auto executable = installedDirectory() / L"CrosshairNative.Engine.exe";
        auto command = L"\"" + executable.wstring() + L"\" --self-test " + std::wstring(token);
        STARTUPINFOW start{};
        start.cb = sizeof(start);
        PROCESS_INFORMATION child{};
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                            &start, &child))
            throw std::runtime_error("Engine launch");
        process = child.hProcess;
        CloseHandle(child.hThread);
    }
    void wait() {
        if (WaitForSingleObject(process, 5000) != WAIT_OBJECT_0)
            throw std::runtime_error("Engine shutdown timeout");
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        if (code)
            throw std::runtime_error("Engine abnormal exit");
        CloseHandle(process);
        process = nullptr;
    }
};
int main(int argc, char** argv) {
    try {
        init_apartment();
        GUID guid{};
        check_hresult(CoCreateGuid(&guid));
        wchar_t raw[40]{};
        StringFromGUID2(guid, raw, 40);
        std::wstring token(raw + 1, 36);
        for (auto& c : token)
            if (c >= L'A' && c <= L'F')
                c += L'a' - L'A';
        const auto directory = std::filesystem::temp_directory_path() / (L"CrosshairNative.Test." + token);
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Test directory already exists");
        unsigned requestId = 0;
        const auto suffix = L".test." + token;
        auto call = [&](const wchar_t* command, JsonObject payload = JsonObject(), bool success = true) {
            JsonObject request;
            request.Insert(L"version", JsonValue::CreateNumberValue(1));
            request.Insert(L"id", JsonValue::CreateNumberValue(++requestId));
            request.Insert(L"command", JsonValue::CreateStringValue(command));
            request.Insert(L"payload", payload);
            const auto reply =
                JsonObject::Parse(to_hstring(ipc::call(to_string(request.Stringify()), suffix)));
            if (reply.GetNamedBoolean(L"ok") != success)
                throw std::runtime_error("Unexpected engine response: " + to_string(reply.Stringify()));
            return success ? reply.GetNamedObject(L"result") : JsonObject();
        };
        Child child;
        child.start(token);
        const auto pipe = ipc::pipeName(suffix);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        auto ready = [&] {
            while (!WaitNamedPipeW(pipe.c_str(), 100)) {
                if (std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("Engine IPC not ready");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        };
        ready();
        auto state = call(L"GetState");
        if (state.GetNamedBoolean(L"visible") || state.GetNamedBoolean(L"eligible"))
            throw std::runtime_error("Unconfigured overlay visible");
        if (argc == 2 && std::string_view(argv[1]) == "--benchmark") {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto cpu = [&] {
                FILETIME created{}, exited{}, kernel{}, user{};
                if (!GetProcessTimes(child.process, &created, &exited, &kernel, &user))
                    throw std::runtime_error("CPU sample");
                return ((static_cast<unsigned long long>(kernel.dwHighDateTime) << 32) |
                        kernel.dwLowDateTime) +
                       ((static_cast<unsigned long long>(user.dwHighDateTime) << 32) | user.dwLowDateTime);
            };
            auto memory = [&] {
                PROCESS_MEMORY_COUNTERS_EX value{};
                value.cb = sizeof(value);
                if (!GetProcessMemoryInfo(child.process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&value),
                                          sizeof(value)))
                    throw std::runtime_error("Memory sample");
                return value.PrivateUsage;
            };
            DWORD handlesBefore = 0, handlesAfter = 0;
            GetProcessHandleCount(child.process, &handlesBefore);
            const auto memoryBefore = memory(), cpuBefore = cpu();
            const auto started = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::seconds(60));
            const auto seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            SYSTEM_INFO system{};
            GetSystemInfo(&system);
            GetProcessHandleCount(child.process, &handlesAfter);
            std::cout << "Idle waiting, isolated engine, UI closed; seconds=" << seconds
                      << "; logicalProcessors=" << system.dwNumberOfProcessors << "; CPU_percent_system="
                      << (cpu() - cpuBefore) / 10000000.0 / seconds / system.dwNumberOfProcessors * 100
                      << "; privateBytes_before=" << memoryBefore << "; privateBytes_after=" << memory()
                      << "; handles_before=" << handlesBefore << "; handles_after=" << handlesAfter << '\n';
        }
        JsonObject preset;
        auto custom = starterPresets()[1];
        custom.id = "user-test-cross";
        custom.layers[0].rotation = 30;
        preset.Insert(L"preset", JsonObject::Parse(to_hstring(encodePreset(custom))));
        call(L"SavePreset", preset);
        call(L"SavePreset", preset, false);
        auto profile = initialProfile();
        profile.id = "profile-a";
        profile.name = "Profilo A";
        profile.slots[0].presetId = custom.id;
        auto save = [&](bool expected = true) {
            JsonObject p;
            p.Insert(L"profile", JsonObject::Parse(to_hstring(encodeProfile(profile))));
            p.Insert(L"configRevision", state.GetNamedValue(L"configRevision"));
            call(L"SaveProfile", p, expected);
            if (expected)
                state = call(L"GetState");
        };
        save();
        auto stale = state;
        profile.id = "profile-b";
        profile.name = "Profilo B";
        save();
        state = stale;
        save(false);
        state = call(L"GetState");
        JsonObject select;
        select.Insert(L"profileId", JsonValue::CreateStringValue(L"profile-a"));
        select.Insert(L"configRevision", state.GetNamedValue(L"configRevision"));
        call(L"SwitchProfile", select);
        state = call(L"GetState");
        if (state.GetNamedObject(L"profile").GetNamedString(L"name") != L"Profilo A")
            throw std::runtime_error("Profile switch");
        JsonObject favorite;
        favorite.Insert(L"presetId", JsonValue::CreateStringValue(L"user-test-cross"));
        favorite.Insert(L"favorite", JsonValue::CreateBooleanValue(true));
        call(L"SetFavorite", favorite);
        call(L"MarkRecent", favorite);
        custom.name = "Mirino modificato";
        custom.layers[0].rotation = 60;
        preset.Insert(L"preset", JsonObject::Parse(to_hstring(encodePreset(custom))));
        call(L"UpdatePreset", preset);
        const auto customPath = directory / L"presets" / ("preset-" + custom.id + ".json");
        if (encodePreset(decodePreset(readJsonFile(customPath))) != encodePreset(custom))
            throw std::runtime_error("Preset update was not persisted");
        state = call(L"GetState");
        if (decodeProfile(to_string(state.GetNamedObject(L"profile").Stringify())).slots[0].presetId != custom.id)
            throw std::runtime_error("Preset update changed slot assignment");
        for (const auto id : {"type-circle", "original-dot", "user-missing"}) {
            auto invalid = custom;
            invalid.id = id;
            JsonObject update;
            update.Insert(L"preset", JsonObject::Parse(to_hstring(encodePreset(invalid))));
            call(L"UpdatePreset", update, false);
        }
        JsonObject theme;
        theme.Insert(L"theme", JsonValue::CreateNumberValue(2));
        call(L"SetTheme", theme);
        state = call(L"GetState");
        if (state.GetNamedArray(L"favorites").Size() != 1 || state.GetNamedArray(L"recents").Size() != 1)
            throw std::runtime_error("Preferences lost");
        call(L"UnknownCommand", JsonObject(), false);
        call(L"Shutdown");
        child.wait();
        child.start(token);
        for (int i = 0; i < 50 && !WaitNamedPipeW(pipe.c_str(), 100); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        state = call(L"GetState");
        if (state.GetNamedNumber(L"theme") != 2 || state.GetNamedArray(L"favorites").Size() != 1)
            throw std::runtime_error("Restart persistence");
        if (encodePreset(decodePreset(readJsonFile(customPath))) != encodePreset(custom))
            throw std::runtime_error("Updated preset lost after restart");
        JsonObject deletion;
        for (const auto id : {L"type-circle", L"original-dot", L"../user-invalid", L"user-missing"}) {
            deletion.Insert(L"presetId", JsonValue::CreateStringValue(id));
            call(L"DeletePreset", deletion, false);
        }
        deletion.Insert(L"presetId", JsonValue::CreateStringValue(to_hstring(custom.id)));
        call(L"DeletePreset", deletion, false); // Active slot still uses it.
        profile = decodeProfile(to_string(state.GetNamedObject(L"profile").Stringify()));
        profile.slots[0].presetId = "original-dot";
        profile.defaultPresetId = custom.id;
        save();
        call(L"DeletePreset", deletion, false); // The fallback also counts as usage.
        profile.defaultPresetId = "original-dot";
        save();
        call(L"DeletePreset", deletion, false); // Saved profile B still uses it.
        profile.id = "profile-b";
        profile.name = "Profilo B";
        save();
        call(L"DeletePreset", deletion);
        if (std::filesystem::exists(directory / L"presets" / ("preset-" + custom.id + ".json")))
            throw std::runtime_error("Deleted preset remains on disk");
        state = call(L"GetState");
        if (state.GetNamedArray(L"favorites").Size() || state.GetNamedArray(L"recents").Size())
            throw std::runtime_error("Deleted preset remains in preferences");
        call(L"DeletePreset", deletion, false);
        call(L"Shutdown");
        child.wait();
        const auto active = directory / L"profiles" / L"active.json";
        {
            std::ofstream corrupt(active);
            corrupt << "{broken";
        }
        child.start(token);
        for (int i = 0; i < 50 && !WaitNamedPipeW(pipe.c_str(), 100); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        state = call(L"GetState");
        if (state.GetNamedBoolean(L"writable"))
            throw std::runtime_error("Corrupt profile writable");
        save(false);
        if (readJsonFile(active) != "{broken")
            throw std::runtime_error("Corrupt original overwritten");
        JsonObject recovery;
        recovery.Insert(L"configRevision", state.GetNamedValue(L"configRevision"));
        call(L"RecoverProfile", recovery, false);
        recovery.Insert(L"confirmed", JsonValue::CreateBooleanValue(true));
        call(L"RecoverProfile", recovery);
        state = call(L"GetState");
        if (!state.GetNamedBoolean(L"writable") || !loadProfile(active).writable)
            throw std::runtime_error("Controlled recovery failed");
        bool preserved = false;
        for (const auto& file : std::filesystem::directory_iterator(active.parent_path()))
            if (file.path().filename().wstring().starts_with(L"active.json.corrupt.") &&
                readJsonFile(file.path()) == "{broken")
                preserved = true;
        if (!preserved)
            throw std::runtime_error("Recovery lost corrupt original");
        call(L"Shutdown");
        child.wait();
        for (int iteration = 0; iteration < 5; ++iteration) {
            child.start(token);
            for (int i = 0; i < 50 && !WaitNamedPipeW(pipe.c_str(), 100); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            call(L"Shutdown");
            child.wait();
        }
        const auto absolute = std::filesystem::absolute(directory).lexically_normal();
        if (!std::filesystem::equivalent(absolute.parent_path(), std::filesystem::temp_directory_path()) ||
            !absolute.filename().wstring().starts_with(L"CrosshairNative.Test."))
            throw std::runtime_error("Unsafe cleanup path");
        rejectReparsePath(absolute);
        std::filesystem::remove_all(absolute);
        std::cout << "Real isolated engine: IPC, custom preset, profiles, revision conflicts, preferences, "
                     "restart, corruption preservation and shutdown passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (const hresult_error& e) {
        std::cerr << to_string(e.message()) << '\n';
        return 1;
    }
}
