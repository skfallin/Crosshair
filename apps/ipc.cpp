#include "apps/ipc.hpp"
#include <sddl.h>
#include <array>
#include <stdexcept>

namespace crosshair::ipc {
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    explicit Handle(HANDLE handle) : value(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
std::wstring logonSid() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw))
        throw std::runtime_error("Identità IPC non disponibile.");
    Handle token(raw);
    DWORD bytes = 0;
    GetTokenInformation(token.value, TokenLogonSid, nullptr, 0, &bytes);
    std::vector<std::byte> buffer(bytes);
    if (!GetTokenInformation(token.value, TokenLogonSid, buffer.data(), bytes, &bytes))
        throw std::runtime_error("Sessione IPC non disponibile.");
    const auto groups = reinterpret_cast<const TOKEN_GROUPS*>(buffer.data());
    LPWSTR text = nullptr;
    if (groups->GroupCount != 1 || !ConvertSidToStringSidW(groups->Groups[0].Sid, &text))
        throw std::runtime_error("SID IPC non disponibile.");
    const std::wstring result(text);
    LocalFree(text);
    return result;
}
DWORD finish(HANDLE pipe, OVERLAPPED& operation, HANDLE stop, bool pending, DWORD immediate) {
    if (!pending)
        return immediate;
    const std::array<HANDLE, 2> events{operation.hEvent, stop};
    const DWORD waited = WaitForMultipleObjects(stop ? 2 : 1, events.data(), FALSE, 3000);
    if (waited != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &operation);
        DWORD ignored = 0;
        GetOverlappedResult(pipe, &operation, &ignored, TRUE);
        throw std::runtime_error("Il motore non risponde. Riprova aggiornando lo stato.");
    }
    DWORD transferred = 0;
    if (!GetOverlappedResult(pipe, &operation, &transferred, FALSE))
        throw std::runtime_error("Connessione al motore interrotta.");
    return transferred;
}
void transfer(HANDLE pipe, void* data, DWORD count, bool write, HANDLE stop = nullptr) {
    auto bytes = static_cast<std::byte*>(data);
    while (count) {
        Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event.value)
            throw std::runtime_error("Risorse IPC non disponibili.");
        OVERLAPPED op{};
        op.hEvent = event.value;
        DWORD done = 0;
        const BOOL ok =
            write ? WriteFile(pipe, bytes, count, &done, &op) : ReadFile(pipe, bytes, count, &done, &op);
        const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
        if (!ok && error != ERROR_IO_PENDING)
            throw std::runtime_error("Connessione al motore interrotta.");
        done = finish(pipe, op, stop, !ok, done);
        if (!done)
            throw std::runtime_error("Messaggio IPC incompleto.");
        count -= done;
        bytes += done;
    }
}
std::string read(HANDLE pipe, HANDLE stop = nullptr) {
    DWORD length = 0;
    transfer(pipe, &length, sizeof(length), false, stop);
    if (!length || length > maxPayload)
        throw std::runtime_error("Payload IPC fuori limite.");
    std::string text(length, '\0');
    transfer(pipe, text.data(), length, false, stop);
    return text;
}
void write(HANDLE pipe, std::string_view text, HANDLE stop = nullptr) {
    if (text.empty() || text.size() > maxPayload)
        throw std::runtime_error("Payload IPC fuori limite.");
    DWORD length = static_cast<DWORD>(text.size());
    transfer(pipe, &length, sizeof(length), true, stop);
    transfer(pipe, const_cast<char*>(text.data()), length, true, stop);
}
} // namespace
std::wstring pipeName(std::wstring_view suffix) {
    return L"\\\\.\\pipe\\CrosshairNative." + logonSid() + L".v1" + std::wstring(suffix);
}
std::string call(std::string_view request, std::wstring_view suffix) {
    const auto name = pipeName(suffix);
    WaitNamedPipeW(name.c_str(), 1000);
    Handle pipe(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED, nullptr));
    if (pipe.value == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Motore non disponibile. Premi Avvia motore e riprova. Codice " +
                                 std::to_string(GetLastError()));
    write(pipe.value, request);
    auto response = read(pipe.value);
    std::byte acknowledgement{1};
    transfer(pipe.value, &acknowledgement, 1, true);
    return response;
}
Server::Server(HWND owner, std::wstring suffix)
    : owner_(owner), suffix_(std::move(suffix)), stop_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
    if (!stop_)
        throw std::runtime_error("Avvio IPC non riuscito.");
    const auto name = pipeName(suffix_);
    const auto sddl = L"D:P(A;;GA;;;" + logonSid() + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor,
                                                              nullptr)) {
        CloseHandle(stop_);
        throw std::runtime_error("ACL IPC non disponibile.");
    }
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    pipe_ = CreateNamedPipeW(name.c_str(),
                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                             maxPayload, maxPayload, 1000, &attributes);
    const auto error = GetLastError();
    LocalFree(descriptor);
    if (pipe_ == INVALID_HANDLE_VALUE) {
        CloseHandle(stop_);
        throw std::runtime_error("Avvio pipe non riuscito. Codice " + std::to_string(error));
    }
    try {
        thread_ = std::jthread([this] { run(); });
    } catch (...) {
        CloseHandle(pipe_);
        CloseHandle(stop_);
        throw;
    }
}
Server::~Server() {
    SetEvent(stop_);
    if (thread_.joinable())
        thread_.join();
    CloseHandle(pipe_);
    CloseHandle(stop_);
}
void Server::run() {
    try {
        // One listening instance remains alive between requests; otherwise a
        // fast reconnect can fall into a FILE_NOT_FOUND gap between instances.
        struct BorrowedPipe {
            HANDLE value;
        } pipe{pipe_};
        while (WaitForSingleObject(stop_, 0) != WAIT_OBJECT_0) {
            struct Disconnect {
                HANDLE pipe;
                ~Disconnect() {
                    DisconnectNamedPipe(pipe);
                }
            } disconnect{pipe.value};
            try {
                Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
                OVERLAPPED op{};
                op.hEvent = event.value;
                const BOOL connected = ConnectNamedPipe(pipe.value, &op);
                const DWORD error = connected ? ERROR_SUCCESS : GetLastError();
                if (!connected && error != ERROR_PIPE_CONNECTED && error != ERROR_IO_PENDING)
                    continue;
                if (error == ERROR_IO_PENDING) {
                    // Idle connection wait has no polling or periodic wakeup.
                    HANDLE events[] = {event.value, stop_};
                    if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0) {
                        CancelIoEx(pipe.value, &op);
                        DWORD ignored = 0;
                        GetOverlappedResult(pipe.value, &op, &ignored, TRUE);
                        break;
                    }
                    DWORD ignored = 0;
                    if (!GetOverlappedResult(pipe.value, &op, &ignored, FALSE))
                        continue;
                }
                ULONG client = 0;
                DWORD clientSession = 0, ownSession = 0;
                if (!GetNamedPipeClientProcessId(pipe.value, &client) ||
                    !ProcessIdToSessionId(client, &clientSession) ||
                    !ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) || clientSession != ownSession)
                    continue;
                auto job = std::make_shared<Job>();
                job->request = read(pipe.value, stop_);
                job->processId = client;
                auto reply = job->reply.get_future();
                {
                    std::lock_guard lock(mutex_);
                    std::erase_if(jobs_, [](const auto& pending) { return pending->cancelled.load(); });
                    jobs_.push_back(job);
                }
                PostMessageW(owner_, dispatchMessage, 0, 0);
                if (reply.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
                    write(pipe.value, reply.get(), stop_);
                    // DisconnectNamedPipe discards unread data. A bounded ACK
                    // replaces an unbounded FlushFileBuffers wait on the client.
                    std::byte acknowledgement{};
                    transfer(pipe.value, &acknowledgement, 1, false, stop_);
                } else
                    job->cancelled = true;
            } catch (const std::exception&) { /* Malformed/idle client is disconnected; no payload logging. */
            }
            if (shutdown_) {
                // Let the client consume the final response before the owner destroys this server.
                // A disconnected client still permits shutdown after the bounded I/O timeout.
                PostMessageW(owner_, shutdownMessage, 0, 0);
                break;
            }
        }
    } catch (const std::exception&) { /* Caller sees a failed connection; no arbitrary fallback channel. */
    }
}
void Server::dispatch(const std::function<std::string(std::string_view, DWORD)>& handler) {
    std::vector<std::shared_ptr<Job>> jobs;
    {
        std::lock_guard lock(mutex_);
        jobs.swap(jobs_);
    }
    for (const auto& job : jobs)
        if (!job->cancelled) {
            try {
                job->reply.set_value(handler(job->request, job->processId));
            } catch (...) {
                job->reply.set_exception(std::current_exception());
            }
        }
}
} // namespace crosshair::ipc
