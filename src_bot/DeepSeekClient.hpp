#pragma once
/*
 * DeepSeekClient.hpp
 *
 * Lightweight header-only interface declaration for interacting with the
 * DeepSeek API (chat/completions style). The implementation should live in
 * a corresponding DeepSeekClient.cpp file.
 *

 *
 * Minimal usage example (synchronous):
 *
 *    #include "DeepSeekClient.hpp"
 *    DeepSeekClient client; // uses DEEPSEEK_API_KEY from env
 *    std::vector<DeepSeekClient::Message> msgs = {
 *        {"system", "You are a concise assistant."},
 *        {"user",   "Explain epoll briefly."}
 *    };
 *    std::string reply = client.complete(msgs);
 *    std::cout << reply << std::endl;
 *
 * Streaming example:
 *
 *    DeepSeekClient::Options opt;
 *    opt.stream = true;
 *    client.completeStream(msgs, opt, [](const DeepSeekClient::ResponseChunk &chunk){
 *        std::cout << chunk.content << std::flush;
 *        if (chunk.done) std::cout << std::endl;
 *    });
 *
 * Thread-safety:
 *  - Distinct instances are independent.
 *  - A single instance is NOT inherently thread-safe; guard externally
 *    if sharing across threads.
 *
 * Error handling:
 *  - Methods throw std::runtime_error (or subclasses) on failure.
 *
 * Dependencies (implementation side suggestions):
 *  - libcurl (preferred) or raw POSIX sockets for HTTPS (with an external TLS lib).
 *  - A minimal JSON library (nlohmann/json or your own lightweight builder).
 *
 * This header deliberately avoids pulling in heavy dependencies so that
 * inclusion cost is minimal. Implementation should keep compile times sane.
 */

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>
#include <cstdlib>

class DeepSeekClient {
public:
    // A single chat-style message.
    struct Message {
        std::string role;     // "system" | "user" | "assistant"
        std::string content;  // UTF-8 text
    };

    // Configuration / tuning knobs for a request.
    struct Options {
        std::string model = "deepseek-chat"; // Adjust to actual deployed model name.
        std::string apiBase = "https://api.deepseek.com"; // Base URL (no trailing slash).
        unsigned    timeoutSeconds = 30;     // Network timeout.
        bool        stream = false;          // Whether to request server-sent event streaming.
        double      temperature = 0.7;       // Typical sampling temperature.
        double      top_p = 1.0;             // Nucleus sampling.
        int         max_tokens = 512;        // Truncation / generation length cap.
        bool        debugLog = false;        // Enable verbose internal logging (implementation-defined).
        // Extend as needed: frequency_penalty, presence_penalty, stop sequences, etc.
    };

    // A chunk of a streaming response.
    struct ResponseChunk {
        std::string content;  // Partial or final content piece.
        bool        done = false; // True when the stream is finished.
    };

    using StreamCallback = std::function<void(const ResponseChunk&)>;

    /*
     * Construct a client.
     * If apiKey is empty, attempts lookup from the provided environment variable name.
     */
    explicit DeepSeekClient(std::string apiKey = "sk-11de78527fc6478194ef6eaef3b12ce5");

    /*
     * Perform a synchronous completion request.
     * Throws std::runtime_error on network / protocol / JSON errors.
     * Returns assistant response as a single consolidated string (first choice).
     */
    std::string complete(const std::vector<Message>& messages, const Options& opt);
    std::string complete(const std::vector<Message>& messages);

    /*
     * Perform a streaming completion request.
     * - opt.stream must be true (will be enforced; implementation may set automatically).
     * - Invokes callback for each incremental content piece.
     * - The final callback invocation will have chunk.done = true.
     */
    void completeStream(const std::vector<Message>& messages,
                        const Options& opt,
                        StreamCallback onChunk);

    /*
     * Update the stored API key at runtime (e.g., key rotation).
     */
    void setApiKey(const std::string& key) { _apiKey = key; }

    /*
     * Access the currently configured API key (may be empty).
     */
    const std::string& apiKey() const { return _apiKey; }

private:
    std::string _apiKey;

    // --- Internal helper declarations (implemented in the .cpp) ---

    // Build JSON payload from messages+options (returns serialized string).
    static std::string _buildPayload(const std::vector<Message>& messages,
                                     const Options& opt);

    // Non-streaming HTTP POST; returns full response body.
    static std::string _httpPost(const std::string& url,
                                 const std::string& jsonPayload,
                                 const Options& opt,
                                 const std::string& apiKey);

    // Streaming HTTP POST; parses SSE/line-delimited JSON and invokes callback.
    static void _httpPostStream(const std::string& url,
                                const std::string& jsonPayload,
                                const Options& opt,
                                const std::string& apiKey,
                                StreamCallback cb);

    // Extract assistant content from a JSON body (non-streaming).
    static std::string _extractContent(const std::string& rawResponse,
                                       const Options& opt);

    // Dispatch streaming lines (e.g., "data: {...}") to callback.
    static void _dispatchStreamPayload(const std::string& line,
                                       StreamCallback cb);

    // Utility: validate a minimal set of messages.
    static void _validateMessages(const std::vector<Message>& messages);

    // Utility: throw if api key missing.
    static void _ensureKeyPresent(const std::string& key);
};
inline std::string DeepSeekClient::complete(const std::vector<DeepSeekClient::Message>& messages) {
    return complete(messages, Options());
}

/*
 * SECURITY NOTE:
 *  Do not commit any real API key, tokens, or secrets into this repository.
 *  Use environment variables or an external secrets manager.
 *
 * EXTENSION POINTS:
 *  - Add automatic retries with backoff.
 *  - Add logging hooks.
 *  - Add JSON library abstraction (if you want to swap implementations).
 *  - Add rate limit handling (sleep + retry logic).
 *
 * TESTING STRATEGY (suggested):
 *  - Provide a mock implementation of _httpPost / _httpPostStream in unit tests.
 *  - Inject mock via linker substitution or refactor into an interface if desired.
 */
