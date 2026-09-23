// Windows (WinHTTP) and Linux (curl child process) implementations of
// httpGet. macOS lives in http_fetch_mac.mm.
#include "http_fetch.h"

#if _WIN32
#include <windows.h>
#include <winhttp.h>
#include <vector>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace MonkSynth {

#if _WIN32

static std::wstring widen(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

namespace {
// Closes a WinHTTP handle on scope exit.
struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET handle) : h(handle) {}
    ~Handle() {
        if (h)
            WinHttpCloseHandle(h);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    explicit operator bool() const { return h != nullptr; }
};
} // namespace

static std::string lastError(const char *what) {
    return std::string(what) + " failed (" + std::to_string(GetLastError()) + ")";
}

HttpResult httpGet(const std::string &url, std::size_t maxBytes) {
    HttpResult result;
    std::wstring wurl = widen(url);

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        result.error = "Bad URL";
        return result;
    }

    // Automatic proxy needs Windows 8.1; fall back to the WinHTTP default.
    Handle session(WinHttpOpen(L"MonkSynth", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
        session.h = WinHttpOpen(L"MonkSynth", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        result.error = lastError("WinHttpOpen");
        return result;
    }
    WinHttpSetTimeouts(session.h, 15000, 15000, 20000, 20000);

    Handle connect(WinHttpConnect(session.h, host, parts.nPort, 0));
    if (!connect) {
        result.error = lastError("WinHttpConnect");
        return result;
    }
    Handle request(WinHttpOpenRequest(connect.h, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) {
        result.error = lastError("WinHttpOpenRequest");
        return result;
    }
    if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA,
                            0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr)) {
        result.error = "Could not connect (" + std::to_string(GetLastError()) + ")";
        return result;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        result.error = "HTTP " + std::to_string(status);
        return result;
    }

    std::vector<char> buf(64 * 1024);
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request.h, buf.data(), static_cast<DWORD>(buf.size()), &read)) {
            result.error = lastError("Download");
            result.body.clear();
            return result;
        }
        if (read == 0)
            break;
        if (result.body.size() + read > maxBytes) {
            result.error = "Response too large";
            result.body.clear();
            return result;
        }
        result.body.append(buf.data(), read);
    }
    result.ok = true;
    return result;
}

#else // Linux (and any other POSIX without NSURLSession)

HttpResult httpGet(const std::string &url, std::size_t maxBytes) {
    HttpResult result;
    if (url.rfind("https://", 0) != 0) {
        result.error = "Bad URL";
        return result;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        result.error = "pipe failed";
        return result;
    }
    // Keep the read end out of the child and of any process the host spawns.
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    // No shell: the URL is passed as a single argument. HTTPS only, including
    // redirects; -f turns HTTP errors into a non-zero exit.
    std::string maxSize = std::to_string(maxBytes);
    const char *argv[] = {"curl",          "-fsSL",   "--proto",   "=https",
                          "--proto-redir", "=https",  "--connect-timeout", "15",
                          "--max-time",    "120",     "--max-filesize",    maxSize.c_str(),
                          url.c_str(),     nullptr};
    pid_t pid = 0;
    int rc = posix_spawnp(&pid, "curl", &actions, nullptr, const_cast<char *const *>(argv),
                          environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (rc != 0) {
        close(fds[0]);
        result.error = "curl is not installed";
        return result;
    }

    char buf[64 * 1024];
    bool tooLarge = false;
    for (;;) {
        ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        if (result.body.size() + static_cast<size_t>(n) > maxBytes) {
            tooLarge = true;
            kill(pid, SIGTERM);
            break;
        }
        result.body.append(buf, static_cast<size_t>(n));
    }
    close(fds[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (tooLarge) {
        result.body.clear();
        result.error = "Response too large";
    } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        result.body.clear();
        int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        result.error = code == 127 ? "curl is not installed" : "Download failed (curl " + std::to_string(code) + ")";
    } else {
        result.ok = true;
    }
    return result;
}

#endif

} // namespace MonkSynth
