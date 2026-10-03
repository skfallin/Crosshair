#pragma once
#include <windows.h>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace crosshair::ipc {
inline constexpr UINT dispatchMessage = WM_APP + 20;
inline constexpr UINT shutdownMessage = WM_APP + 21;
inline constexpr DWORD maxPayload = 131072;
std::wstring pipeName(std::wstring_view suffix = {});
std::string call(std::string_view request, std::wstring_view suffix = {});
class Server {
  public:
    explicit Server(HWND owner, std::wstring suffix = {});
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    void dispatch(const std::function<std::string(std::string_view, DWORD)>& handler);
    void shutdownAfterReply() {
        shutdown_ = true;
    }

  private:
    struct Job {
        std::string request;
        DWORD processId = 0;
        std::promise<std::string> reply;
        std::atomic_bool cancelled = false;
    };
    HWND owner_;
    std::wstring suffix_;
    HANDLE stop_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::mutex mutex_;
    std::vector<std::shared_ptr<Job>> jobs_;
    std::jthread thread_;
    std::atomic_bool shutdown_ = false;
    void run();
};
} // namespace crosshair::ipc
