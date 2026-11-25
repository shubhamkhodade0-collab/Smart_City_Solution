// =====================================================================
// HttpClient.cpp - see HttpClient.h
// =====================================================================
#include "HttpClient.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace scms {

bool parseUrl(const std::string& url, UrlParts& out) {
    out = UrlParts();
    std::size_t p = url.find("://");
    if (p == std::string::npos) return false;
    out.scheme = url.substr(0, p);
    for (auto& ch : out.scheme) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (out.scheme != "http" && out.scheme != "https") return false;
    out.secure = out.scheme == "https";
    std::size_t hostStart = p + 3;
    std::size_t pathStart = url.find_first_of("/?#", hostStart);
    std::string authority = url.substr(hostStart, pathStart == std::string::npos ? std::string::npos : pathStart - hostStart);
    if (authority.empty() || authority.find('@') != std::string::npos || authority.find('[') != std::string::npos) return false;
    std::size_t colon = authority.rfind(':');
    out.port = out.secure ? 443 : 80;
    if (colon != std::string::npos) {
        std::string port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5) return false;
        for (char ch : port)
            if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
        out.port = std::atoi(port.c_str());
        if (out.port <= 0 || out.port > 65535) return false;
        authority.resize(colon);
    }
    if (authority.empty()) return false;
    out.host = authority;
    std::string rest = pathStart == std::string::npos ? "" : url.substr(pathStart);
    std::size_t frag = rest.find('#');
    if (frag != std::string::npos) rest.resize(frag);
    if (rest.empty() || rest[0] == '?') rest = "/" + rest;
    out.path = rest;
    return true;
}

} // namespace scms

// =====================================================================
#ifdef _WIN32
// ------------------------------------------------------------------ Windows: WinHTTP
#include <windows.h>
#include <winhttp.h>

namespace scms {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string describe(DWORD code) {
    switch (code) {
        case 12002: return "the request timed out";
        case 12007: return "the server name could not be found - check the internet connection";
        case 12029: return "could not connect to the server - check the internet connection or firewall";
        case 12030: return "the connection was closed unexpectedly";
        case 12175: return "the secure (TLS) connection failed - check the PC date and time and any antivirus HTTPS scanning";
        case 12157: return "the secure (TLS) connection failed";
        case 12044: case 12045: return "the server certificate was not accepted";
        default: return "network error " + std::to_string(static_cast<unsigned long>(code));
    }
}

struct Handle {
    HINTERNET h = nullptr;
    Handle() {}
    explicit Handle(HINTERNET x) : h(x) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

} // namespace

HttpResponse httpSend(const HttpRequest& req) {
    HttpResponse res;
    UrlParts u;
    if (!parseUrl(req.url, u)) { res.error = "invalid URL"; return res; }

#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    Handle session(WinHttpOpen(L"SmartCity/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.h)   // older Windows: fall back to the machine-wide proxy setting
        session.h = WinHttpOpen(L"SmartCity/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
#else
    Handle session(WinHttpOpen(L"SmartCity/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
#endif
    if (!session.h) { res.error = describe(GetLastError()); return res; }

    const int ms = req.timeoutSec * 1000;
    WinHttpSetTimeouts(session.h, 10000, 10000, ms, ms);
    DWORD protocols = 0x00000800 | 0x00002000;   // TLS 1.2 | TLS 1.3 (ignored where unsupported)
    WinHttpSetOption(session.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);

    Handle connection(WinHttpConnect(session.h, widen(u.host).c_str(), static_cast<INTERNET_PORT>(u.port), 0));
    if (!connection.h) { res.error = describe(GetLastError()); return res; }

    Handle request(WinHttpOpenRequest(connection.h, widen(req.method).c_str(), widen(u.path).c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, u.secure ? WINHTTP_FLAG_SECURE : 0));
    if (!request.h) { res.error = describe(GetLastError()); return res; }

    for (const auto& h : req.headers) {
        std::wstring line = widen(h.first + ": " + h.second);
        WinHttpAddRequestHeaders(request.h, line.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    DWORD len = static_cast<DWORD>(req.body.size());
    LPVOID data = req.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(req.body.data());
    if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, data, len, len, 0) || !WinHttpReceiveResponse(request.h, nullptr)) {
        res.error = describe(GetLastError());
        return res;
    }

    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    res.status = static_cast<int>(status);

    static const std::size_t MAX_BODY = 4 * 1024 * 1024;   // an answer is a few KB; refuse anything absurd
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request.h, &avail)) { res.error = describe(GetLastError()); break; }
        if (avail == 0) break;
        std::string chunk(avail, '\0');
        DWORD got = 0;
        if (!WinHttpReadData(request.h, &chunk[0], avail, &got)) { res.error = describe(GetLastError()); break; }
        res.body.append(chunk, 0, got);
        if (res.body.size() > MAX_BODY) { res.error = "the answer was too large"; break; }
    }
    return res;
}

} // namespace scms

#else
// ------------------------------------------------------------------ other systems: the curl tool
#include <unistd.h>

namespace scms {
namespace {

std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

// the key must not appear on a command line (visible in the process list): headers and body go through files
struct TempFile {
    std::string path;
    bool create(const std::string& content) {
        char tmpl[] = "/tmp/scms_http_XXXXXX";
        int fd = mkstemp(tmpl);   // created with mode 0600
        if (fd < 0) return false;
        path = tmpl;
        std::size_t off = 0;
        while (off < content.size()) {
            ssize_t n = write(fd, content.data() + off, content.size() - off);
            if (n <= 0) break;
            off += static_cast<std::size_t>(n);
        }
        close(fd);
        return off == content.size();
    }
    ~TempFile() { if (!path.empty()) std::remove(path.c_str()); }
};

} // namespace

HttpResponse httpSend(const HttpRequest& req) {
    HttpResponse res;
    UrlParts u;
    if (!parseUrl(req.url, u)) { res.error = "invalid URL"; return res; }
    for (char c : req.method)
        if (!std::isalpha(static_cast<unsigned char>(c))) { res.error = "invalid method"; return res; }

    std::string headerText;
    for (const auto& h : req.headers) headerText += h.first + ": " + h.second + "\n";
    TempFile headers, body;
    if (!headers.create(headerText) || (!req.body.empty() && !body.create(req.body))) { res.error = "could not create a temporary file"; return res; }

    std::string cmd = "curl -sS --max-time " + std::to_string(req.timeoutSec) + " --connect-timeout 10 -X " + req.method + " " + shellQuote(req.url) +
                      " -H @" + shellQuote(headers.path);
    if (!req.body.empty()) cmd += " --data-binary @" + shellQuote(body.path);
    cmd += " -w '\\n%{http_code}' 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { res.error = "could not start curl (is it installed?)"; return res; }
    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = fread(buf, 1, sizeof buf, pipe)) > 0) out.append(buf, n);
    int rc = pclose(pipe);

    std::size_t nl = out.rfind('\n');
    std::string tail = nl == std::string::npos ? out : out.substr(nl + 1);
    bool digits = tail.size() == 3;
    for (char c : tail) digits = digits && std::isdigit(static_cast<unsigned char>(c));
    int code = digits ? std::atoi(tail.c_str()) : 0;
    if (code > 0) {
        res.status = code;
        res.body = out.substr(0, nl);
    } else {
        std::string msg = digits && nl != std::string::npos ? out.substr(0, nl) : out;   // drop the "000" status marker
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == ' ')) msg.pop_back();
        if (msg.size() > 200) msg.resize(200);
        res.error = msg.empty() ? "curl failed (exit " + std::to_string(rc) + ")" : msg;
    }
    return res;
}

} // namespace scms
#endif
