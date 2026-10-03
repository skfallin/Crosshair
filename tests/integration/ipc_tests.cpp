#include "apps/ipc.hpp"
#include <iostream>
#include <future>

using namespace crosshair;
namespace {
ipc::Server* server = nullptr;
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == ipc::dispatchMessage && server) {
        server->dispatch([](std::string_view input, DWORD pid) {
            if (pid != GetCurrentProcessId())
                throw std::runtime_error("Client PID");
            return std::string(input);
        });
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
} // namespace
int main() {
    try {
        WNDCLASSW wc{};
        wc.lpfnWndProc = procedure;
        wc.lpszClassName = L"CrosshairNative.IpcTest";
        wc.hInstance = GetModuleHandleW(nullptr);
        if (!RegisterClassW(&wc))
            return 1;
        const auto window =
            CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        const auto suffix = L".test." + std::to_wstring(GetCurrentProcessId());
        {
            ipc::Server instance(window, suffix);
            server = &instance;
            auto client = std::async(std::launch::async, [suffix] {
                // Startup synchronization only; normal clients reconnect explicitly.
                Sleep(100);
                for (int i = 0; i < 20; ++i) {
                    const auto request = std::string(20000, 'a');
                    if (ipc::call(request, suffix) != request)
                        throw std::runtime_error("Truncated reply");
                }
                bool rejected = false;
                try {
                    ipc::call(std::string(ipc::maxPayload + 1, 'x'), suffix);
                } catch (const std::exception&) {
                    rejected = true;
                }
                if (!rejected)
                    throw std::runtime_error("Oversized payload accepted");
            });
            const auto deadline = GetTickCount64() + 15000;
            while (client.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                if (GetTickCount64() > deadline)
                    throw std::runtime_error("IPC timeout");
                MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
            }
            client.get();
            server = nullptr;
        }
        DestroyWindow(window);
        std::cout << "20 framed IPC round trips, authenticated PID, payload limit and shutdown passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
