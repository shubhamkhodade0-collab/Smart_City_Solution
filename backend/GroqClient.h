// =====================================================================
// GroqClient.h - OPTIONAL connection to Groq (https://groq.com) so the AI assistant can also answer
//                questions its built-in playbooks do not cover.
//
//   * Off by default. Nothing is sent anywhere until the user adds their own Groq API key.
//   * Groq speaks the OpenAI chat-completions protocol:  POST {base}/chat/completions  (Bearer key)
//   * The key comes from (highest priority first): environment SCMS_GROQ_API_KEY / GROQ_API_KEY,
//     legacy config/groq.conf, .env, then legacy config/app.conf.
//   * Every function here is free of the UI. The network call blocks, so the application runs it on a
//     worker thread through Async<T>.
// =====================================================================
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace scms {
namespace ai {

struct LlmSettings {
    std::string apiKey;
    std::string model = "openai/gpt-oss-20b";            // Groq retires models regularly: this is editable in the dialog
    std::string baseUrl = "https://api.groq.com/openai/v1";
    int mode = 1;                // 1 = direct AI: every message goes to Groq (any topic); 0 = built-in assistant first, Groq only for what it cannot answer
    bool shareDetails = true;    // include the complaint's category, status, location and description in the prompt
    bool remember = false;       // legacy setting: key may be stored in config/groq.conf (plain text)
    std::string keySource = "none";   // none | config/groq.conf | .env | config/app.conf | environment variable
    bool ready() const { return !apiKey.empty(); }
};

// ---- settings (environment + config/groq.conf + .env) ----------------------------------------------------
LlmSettings loadLlmSettings(const std::string& path = "config/groq.conf", const std::string& appConfPath = "config/app.conf",
                            const std::string& envFilePath = ".env");
bool saveLlmSettings(const LlmSettings& s, const std::string& path, std::string* error = nullptr);
bool removeSavedKey(const std::string& path, std::string* error = nullptr);   // deletes the key line, keeps the other settings
std::string cleanApiKey(const std::string& raw);     // trims blanks / quotes / a leading "Bearer "
std::string maskKey(const std::string& key);         // "gsk_...Ab12"
bool baseUrlAllowed(const std::string& url);         // https anywhere; plain http only for localhost (tests)
bool isChatModelId(const std::string& id);           // false for speech / guard / safety models

// ---- one chat request ----------------------------------------------------------------------------------------
struct LlmMessage {
    std::string role;      // "system" | "user" | "assistant"
    std::string content;
};

struct LlmRequest {
    LlmSettings settings;
    std::vector<LlmMessage> messages;   // system prompt first, then the conversation, the new question last
};

struct LlmResult {
    bool ok = false;
    std::string text;          // the answer (when ok)
    std::string error;         // a friendly explanation (when !ok)
    int httpStatus = 0;
    std::string model;
};

struct ModelList {
    bool ok = false;
    std::vector<std::string> models;
    std::string error;
};

std::string buildChatBody(const LlmRequest& request);                           // JSON sent to Groq
LlmResult parseChatResponse(int httpStatus, const std::string& body, const std::string& transportError = "", const std::string& model = "");
std::string friendlyLlmError(int httpStatus, const std::string& serverMessage, const std::string& model);
ModelList parseModelList(int httpStatus, const std::string& body, const std::string& transportError = "");

LlmResult groqChat(const LlmRequest& request);          // blocking - call from a worker thread
ModelList groqListModels(const LlmSettings& settings);  // blocking - GET /models (also tests the key)

// ---- run something on a worker thread, poll from the UI loop -------------------------------------------
template <typename T>
class Async {
public:
    static std::shared_ptr<Async<T>> run(std::function<T()> fn) {
        std::shared_ptr<Async<T>> a = std::make_shared<Async<T>>();
        std::thread([a, fn]() {
            T r = fn();
            std::lock_guard<std::mutex> lock(a->mutex_);
            a->value_ = std::move(r);
            a->done_ = true;
        }).detach();
        return a;
    }
    bool done() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return done_;
    }
    T get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }
    void cancel() { cancelled_ = true; }
    bool cancelled() const { return cancelled_; }

private:
    mutable std::mutex mutex_;
    bool done_ = false;
    std::atomic<bool> cancelled_{false};
    T value_{};
};

} // namespace ai
} // namespace scms
