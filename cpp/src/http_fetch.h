#pragma once

#include <cstddef>
#include <string>

namespace MonkSynth {

struct HttpResult {
    bool ok = false;
    std::string body;
    std::string error; // short, user-facing reason when !ok
};

// Blocking HTTPS GET for the theme gallery. Follows redirects and fails on
// any status other than 200 or a body larger than |maxBytes|. Call it from a
// worker thread: it can take as long as the platform timeouts (tens of
// seconds) on a bad connection.
//
// macOS uses NSURLSession and Windows uses WinHTTP, both part of the OS.
// Linux runs the system curl as a child process rather than linking libcurl,
// which would drag TLS libraries into the statically linked plugin.
HttpResult httpGet(const std::string &url, std::size_t maxBytes);

} // namespace MonkSynth
