// =====================================================================
// HttpClient.h - one blocking HTTP(S) request, no third-party library
//
//   Windows : WinHTTP (part of Windows; uses the system certificate store and proxy settings)
//   others  : the "curl" command line tool (used when developing / testing on Linux or macOS)
//
// Used only by the optional Groq connection. Always call it from a worker thread (it blocks until the
// answer arrives or the timeout is reached).
// =====================================================================
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace scms {

struct UrlParts {
    std::string scheme, host, path;   // path always starts with '/' and includes the query
    int port = 0;
    bool secure = false;
};

// splits "https://host[:port]/path?query"; returns false for anything else (no IPv6 literals, no user info)
bool parseUrl(const std::string& url, UrlParts& out);

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeoutSec = 45;
};

struct HttpResponse {
    int status = 0;        // HTTP status code, 0 when the request failed before an answer arrived
    std::string body;
    std::string error;     // non-empty when the request failed (network error, timeout, TLS problem ...)
    bool ok() const { return error.empty() && status > 0; }
};

HttpResponse httpSend(const HttpRequest& request);

} // namespace scms
