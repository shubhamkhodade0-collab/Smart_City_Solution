// =====================================================================
// GroqClient.cpp - see GroqClient.h
// =====================================================================
#include "GroqClient.h"
#include "AiUtil.h"
#include "HttpClient.h"
#include "MiniJson.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace scms {
namespace ai {

using namespace util;

namespace {

const char* const DEFAULT_BASE = "https://api.groq.com/openai/v1";

std::string envValue(const char* name) {
    const char* v = std::getenv(name);
    return v ? trimCopy(v) : std::string();
}

std::string readEnvFirst(const char* a, const char* b) {
    std::string v = envValue(a);
    return v.empty() ? envValue(b) : v;
}

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

} // namespace

std::string cleanApiKey(const std::string& raw) {
    std::string k = trimCopy(raw);
    while (!k.empty() && (k.front() == '"' || k.front() == '\'')) k.erase(k.begin());
    while (!k.empty() && (k.back() == '"' || k.back() == '\'' || k.back() == ';')) k.pop_back();
    if (lowerCopy(k).rfind("bearer ", 0) == 0) k = trimCopy(k.substr(7));
    for (char c : k)   // a key is one printable token; anything else means a bad paste
        if (uc(c) <= 0x20 || uc(c) >= 0x7F) return std::string();
    return k.size() > 300 ? std::string() : k;
}

std::string maskKey(const std::string& key) {
    if (key.empty()) return "";
    if (key.size() <= 10) return std::string(key.size(), '*');
    return key.substr(0, 4) + "..." + key.substr(key.size() - 4);
}

bool baseUrlAllowed(const std::string& url) {
    if (startsWith(lowerCopy(url), "https://")) return true;
    // plain http is accepted only for a server on this computer (used by the tests); the key never travels unencrypted
    std::string low = lowerCopy(url);
    return startsWith(low, "http://127.0.0.1") || startsWith(low, "http://localhost");
}

bool isChatModelId(const std::string& id) {
    std::string low = lowerCopy(id);
    static const char* skip[] = {"whisper", "guard", "safeguard", "tts", "playai", "orpheus", "embed", "transcri"};
    for (const char* s : skip)
        if (low.find(s) != std::string::npos) return false;
    return !low.empty();
}

// ------------------------------------------------------------------ settings file
// reads the GROQ_* lines of one KEY=VALUE file; returns true when it contained a (non-empty) key
static bool readGroqFile(const std::string& path, LlmSettings& s) {
    std::ifstream in(path);
    std::string line;
    bool hasKey = false;
    while (in && std::getline(in, line)) {
        line = trimCopy(line);
        if (line.empty() || line[0] == '#') continue;
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trimCopy(line.substr(0, eq)), val = trimCopy(line.substr(eq + 1));
        if (key == "GROQ_API_KEY") {
            std::string k = cleanApiKey(val);
            if (!k.empty()) { s.apiKey = k; hasKey = true; }
        }
        else if (key == "GROQ_MODEL" && !val.empty()) s.model = val;
        else if (key == "GROQ_MODE") s.mode = val == "0" ? 0 : 1;
        else if (key == "GROQ_SHARE_DETAILS") s.shareDetails = val != "0";
        else if (key == "GROQ_BASE_URL" && !val.empty()) s.baseUrl = val;
    }
    return hasKey;
}

LlmSettings loadLlmSettings(const std::string& path, const std::string& appConfPath, const std::string& envFilePath) {
    LlmSettings s;
    // Lowest priority first: legacy app.conf, .env, saved dialog settings, then process environment.
    if (!appConfPath.empty() && readGroqFile(appConfPath, s)) s.keySource = "config/app.conf";
    if (!envFilePath.empty() && readGroqFile(envFilePath, s)) s.keySource = ".env";
    if (readGroqFile(path, s)) { s.keySource = "config/groq.conf"; s.remember = true; }

    // environment variables override the file (same rule as config/app.conf)
    std::string envKey = cleanApiKey(readEnvFirst("SCMS_GROQ_API_KEY", "GROQ_API_KEY"));
    if (!envKey.empty()) { s.apiKey = envKey; s.keySource = "environment variable"; s.remember = false; }
    std::string envModel = readEnvFirst("SCMS_GROQ_MODEL", "GROQ_MODEL");
    if (!envModel.empty()) s.model = envModel;
    std::string envBase = envValue("SCMS_GROQ_BASE_URL");
    if (!envBase.empty()) s.baseUrl = envBase;
    while (!s.baseUrl.empty() && s.baseUrl.back() == '/') s.baseUrl.pop_back();
    if (s.baseUrl.empty()) s.baseUrl = DEFAULT_BASE;
    return s;
}

static bool writeFileAtomically(const std::string& path, const std::string& content, std::string* error) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { if (error) *error = "cannot write " + path; return false; }
        out << content;
        if (!out) { if (error) *error = "cannot write " + path; return false; }
    }
#ifndef _WIN32
    chmod(tmp.c_str(), 0600);   // the key is readable by this user only
#endif
    std::remove(path.c_str());   // Windows cannot rename over an existing file
    if (std::rename(tmp.c_str(), path.c_str()) != 0) { if (error) *error = "cannot replace " + path; return false; }
    return true;
}

bool saveLlmSettings(const LlmSettings& s, const std::string& path, std::string* error) {
    std::ostringstream o;
    o << "# SmartCity - optional Groq connection for the AI assistant (written by the application)\n"
      << "# The key below is stored as PLAIN TEXT. Keep this file private; delete the GROQ_API_KEY line to forget it.\n";
    if (s.remember && !s.apiKey.empty() && s.keySource != "environment variable") o << "GROQ_API_KEY=" << s.apiKey << "\n";
    o << "GROQ_MODEL=" << s.model << "\n"
      << "GROQ_MODE=" << s.mode << "\n"
      << "GROQ_SHARE_DETAILS=" << (s.shareDetails ? 1 : 0) << "\n";
    if (s.baseUrl != DEFAULT_BASE) o << "GROQ_BASE_URL=" << s.baseUrl << "\n";
    return writeFileAtomically(path, o.str(), error);
}

bool removeSavedKey(const std::string& path, std::string* error) {
    std::ifstream in(path);
    if (!in) return true;   // nothing saved
    std::ostringstream o;
    std::string line;
    while (std::getline(in, line)) {
        if (trimCopy(line).rfind("GROQ_API_KEY", 0) == 0) continue;
        o << line << "\n";
    }
    in.close();
    return writeFileAtomically(path, o.str(), error);
}

// ------------------------------------------------------------------ request / response
static bool isReasoningModel(const std::string& model) { return startsWith(lowerCopy(model), "openai/gpt-oss"); }

std::string buildChatBody(const LlmRequest& r) {
    std::ostringstream o;
    o << "{\"model\":" << json::quote(r.settings.model) << ",\"messages\":[";
    for (std::size_t i = 0; i < r.messages.size(); ++i) {
        if (i) o << ",";
        o << "{\"role\":" << json::quote(r.messages[i].role) << ",\"content\":" << json::quote(r.messages[i].content) << "}";
    }
    // gpt-oss models "think" before answering and the thinking counts against the token limit: ask for little
    // thinking and leave plenty of room, otherwise the answer can come back empty.
    o << "],\"temperature\":0.3,\"max_completion_tokens\":1500,\"stream\":false";
    if (isReasoningModel(r.settings.model)) o << ",\"reasoning_effort\":\"low\"";
    o << "}";
    return o.str();
}

std::string friendlyLlmError(int status, const std::string& message, const std::string& model) {
    std::string low = lowerCopy(message);
    if (low.find("decommission") != std::string::npos || low.find("deprecat") != std::string::npos || low.find("no longer supported") != std::string::npos)
        return "Groq has retired the model '" + model + "'. Open the Groq settings and choose another model (for example " +
               (model == "openai/gpt-oss-20b" ? "openai/gpt-oss-120b" : "openai/gpt-oss-20b") + ").";
    switch (status) {
        case 401: return "Groq rejected the API key (401). Open the Groq settings and check the key.";
        case 403: return "Groq refused the request (403). The key may not be allowed to use this model.";
        case 404: return "Groq does not know the model '" + model + "' (404). Open the Groq settings and choose another model.";
        case 413: return "The request was too large for Groq.";
        case 429: return "Groq's rate limit was reached (429). Wait a minute and try again.";
        default: break;
    }
    if (status >= 500) return "Groq is temporarily unavailable (" + std::to_string(status) + "). Try again later.";
    std::string m = message.empty() ? "unexpected answer (HTTP " + std::to_string(status) + ")" : message;
    if (m.size() > 240) m.resize(240);
    return m;
}

static std::string serverErrorMessage(const std::string& body) {
    json::Value v;
    if (!json::parse(body, v)) return "";
    const json::Value* e = v.get("error");
    if (e && e->isObject()) {
        const json::Value* m = e->get("message");
        if (m && m->isString()) return m->text;
    }
    if (e && e->isString()) return e->text;
    return "";
}

LlmResult parseChatResponse(int status, const std::string& body, const std::string& transportError, const std::string& model) {
    LlmResult r;
    r.httpStatus = status;
    if (!transportError.empty()) { r.error = "Could not reach Groq: " + transportError; return r; }
    if (status != 200) { r.error = friendlyLlmError(status, serverErrorMessage(body), model); return r; }
    json::Value v;
    std::string perr;
    if (!json::parse(body, v, &perr)) {
        r.error = trimCopy(body).rfind("<", 0) == 0 ? "The answer was a web page, not data from Groq - a proxy, firewall or sign-in page may be in the way."
                                                   : "Groq sent an answer I could not read (" + perr + ").";
        return r;
    }
    if (const json::Value* m = v.get("model")) r.model = m->asString();
    const json::Value* choice = nullptr;
    if (const json::Value* c = v.get("choices")) choice = c->at(0);
    const json::Value* msg = choice ? choice->get("message") : nullptr;
    const json::Value* content = msg ? msg->get("content") : nullptr;
    std::string text = content ? trimCopy(content->asString()) : std::string();
    if (text.empty()) {
        const json::Value* fr = choice ? choice->get("finish_reason") : nullptr;
        r.error = fr && fr->asString() == "length"
                      ? "The model used up its answer length while thinking. Try again, or choose a different model in the Groq settings."
                      : "Groq returned an empty answer. Try again.";
        return r;
    }
    r.ok = true;
    r.text = text;
    return r;
}

ModelList parseModelList(int status, const std::string& body, const std::string& transportError) {
    ModelList m;
    if (!transportError.empty()) { m.error = "Could not reach Groq: " + transportError; return m; }
    if (status != 200) { m.error = friendlyLlmError(status, serverErrorMessage(body), ""); return m; }
    json::Value v;
    std::string perr;
    if (!json::parse(body, v, &perr)) { m.error = "Groq sent a list I could not read (" + perr + ")."; return m; }
    const json::Value* data = v.get("data");
    if (!data || !data->isArray()) { m.error = "Groq sent an unexpected model list."; return m; }
    for (const auto& item : data->items)
        if (const json::Value* id = item.get("id"))
            if (id->isString() && isChatModelId(id->text)) m.models.push_back(id->text);
    m.ok = true;
    return m;
}

static std::vector<std::pair<std::string, std::string>> authHeaders(const LlmSettings& s) {
    return {{"Authorization", "Bearer " + s.apiKey}, {"Content-Type", "application/json"}, {"Accept", "application/json"}};
}

LlmResult groqChat(const LlmRequest& request) {
    LlmResult r;
    const LlmSettings& s = request.settings;
    if (!s.ready()) { r.error = "No Groq API key is set."; return r; }
    if (!baseUrlAllowed(s.baseUrl)) { r.error = "The Groq address must start with https:// (the key is never sent over plain http)."; return r; }
    HttpRequest h;
    h.method = "POST";
    h.url = s.baseUrl + "/chat/completions";
    h.headers = authHeaders(s);
    h.body = buildChatBody(request);
    h.timeoutSec = 60;
    HttpResponse resp = httpSend(h);
    r = parseChatResponse(resp.status, resp.body, resp.error, s.model);
    if (r.model.empty()) r.model = s.model;
    return r;
}

ModelList groqListModels(const LlmSettings& s) {
    ModelList m;
    if (!s.ready()) { m.error = "No Groq API key is set."; return m; }
    if (!baseUrlAllowed(s.baseUrl)) { m.error = "The Groq address must start with https:// (the key is never sent over plain http)."; return m; }
    HttpRequest h;
    h.method = "GET";
    h.url = s.baseUrl + "/models";
    h.headers = authHeaders(s);
    h.timeoutSec = 30;
    HttpResponse resp = httpSend(h);
    return parseModelList(resp.status, resp.body, resp.error);
}

} // namespace ai
} // namespace scms
