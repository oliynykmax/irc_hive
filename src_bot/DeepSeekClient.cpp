#include "DeepSeekClient.hpp"

/*
 * DeepSeekClient.cpp
 *
 * A lightweight implementation for the DeepSeekClient declared in
 * DeepSeekClient.hpp. This code attempts to use libcurl if available,
 * and provides a raw POSIX + OpenSSL fallback when libcurl is not present.
 *
 * NOTE:
 *  - This is a minimal implementation intended for prototyping.
 *  - It does not perform robust JSON parsing; it uses naive string
 *    searches to extract "content". For production, integrate a real
 *    JSON library (e.g., nlohmann/json).
 *  - Streaming handling is basic: it expects server-sent event (SSE)
 *    style lines beginning with "data:" similar to OpenAI's format.
 *  - Error handling is coarse-grained; network / protocol issues
 *    result in std::runtime_error.
 *
 * SECURITY:
 *  - API key must be provided via constructor argument.
 *  - Never hardcode secrets in production code.
 *
 * EXTENDING:
 *  - Add rate limit/backoff strategy.
 *  - Replace naive JSON with a proper parser.
 *  - Add logging abstraction.
 */

#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <map>

#if __has_include(<curl/curl.h>)
	#define DEEPSEEK_HAVE_CURL 1
	#include <curl/curl.h>
#else
	#define DEEPSEEK_HAVE_CURL 0
#endif

#if !DEEPSEEK_HAVE_CURL
	#include <sys/socket.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <sys/wait.h> // added for waitpid used in exec curl fallback
	#if __has_include(<openssl/ssl.h>) && __has_include(<openssl/err.h>)
		#define DEEPSEEK_HAVE_OPENSSL 1
		#include <openssl/ssl.h>
		#include <openssl/err.h>
	#else
		#define DEEPSEEK_HAVE_OPENSSL 0
	#endif
#endif

// -------- Internal utility helpers (local to this translation unit) ------- //

namespace {

static std::string trim(const std::string &s) {
	size_t start = 0;
	while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
	size_t end = s.size();
	while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
	return s.substr(start, end - start);
}

static std::string jsonEscape(const std::string &in) {
	std::string out;
	out.reserve(in.size() + 16);
	for (char c : in) {
		switch (c) {
			case '\\': out += "\\\\"; break;
			case '"':  out += "\\\""; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					char buf[7];
					snprintf(buf, sizeof(buf), "\\u%04x", c & 0xff);
					out += buf;
				} else {
					out += c;
				}
		}
	}
	return out;
}

static std::string unescapeBasic(const std::string &in) {
	// Minimal unescape for the subset we expect. Not robust.
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i) {
		if (in[i] == '\\' && i + 1 < in.size()) {
			char n = in[i + 1];
			switch (n) {
				case 'n': out += '\n'; ++i; continue;
				case 'r': out += '\r'; ++i; continue;
				case 't': out += '\t'; ++i; continue;
				case '\\': out += '\\'; ++i; continue;
				case '"': out += '"'; ++i; continue;
				case 'u':
					// Skip simplistic: copy literally
					out += "\\u";
					++i;
					continue;
				default:
					out += n;
					++i;
					continue;
			}
		}
		out += in[i];
	}
	return out;
}

// Return substring between first occurrence of key":" and the closing quote (naive).
static bool extractFirstJsonStringValue(const std::string &json, const std::string &key, std::string &out) {
	std::string pattern = "\"" + key + "\"";
	size_t pos = json.find(pattern);
	if (pos == std::string::npos) return false;
	pos = json.find(':', pos + pattern.size());
	if (pos == std::string::npos) return false;
	// Skip spaces
	while (pos < json.size() && (json[pos] == ':' || std::isspace(static_cast<unsigned char>(json[pos])))) ++pos;
	if (pos >= json.size() || json[pos] != '"') return false;
	++pos;
	std::string val;
	while (pos < json.size()) {
		char c = json[pos];
		if (c == '\\') {
			if (pos + 1 < json.size()) {
				val += json.substr(pos, 2);
				pos += 2;
				continue;
			} else {
				break;
			}
		} else if (c == '"') {
			out = unescapeBasic(val);
			return true;
		} else {
			val += c;
			++pos;
		}
	}
	return false;
}

// Attempt to extract the first assistant message content from a chat response.
// Very naive: looks for `"role":"assistant"` then nearest `"content":"..."` after it.
static bool extractAssistantContent(const std::string &json, std::string &content) {
	size_t rolePos = json.find("\"role\"");
	while (rolePos != std::string::npos) {
		size_t assistantPos = json.find("\"assistant\"", rolePos);
		if (assistantPos == std::string::npos) return false;
		// Found "assistant" after "role"
		// From assistantPos forward, find "content"
		size_t contentPos = json.find("\"content\"", assistantPos);
		if (contentPos == std::string::npos) {
			// Search next role
			rolePos = json.find("\"role\"", rolePos + 6);
			continue;
		}
		// Extract content value
		size_t colon = json.find(':', contentPos + 9);
		if (colon == std::string::npos) return false;
		size_t start = json.find('"', colon);
		if (start == std::string::npos) return false;
		++start;
		std::string val;
		for (size_t i = start; i < json.size(); ++i) {
			if (json[i] == '\\' && i + 1 < json.size()) {
				val += json.substr(i, 2);
				++i;
				continue;
			}
			if (json[i] == '"') {
				content = unescapeBasic(val);
				return true;
			}
			val += json[i];
		}
		return false;
	}
	return false;
}

#if DEEPSEEK_HAVE_CURL
struct CurlGlobal {
	CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
	~CurlGlobal() { curl_global_cleanup(); }
};
static CurlGlobal g_curlGlobal; // ensure global init
#endif

} // namespace

// --------------------------- DeepSeekClient Impl --------------------------- //

DeepSeekClient::DeepSeekClient(std::string apiKey)
: _apiKey(std::move(apiKey))
{
	// API key is taken as provided (constructor default supplies one).
	// No environment lookup performed here anymore.
}

void DeepSeekClient::_validateMessages(const std::vector<Message>& messages) {
	if (messages.empty())
		throw std::runtime_error("DeepSeekClient: messages vector is empty");
	for (const auto &m : messages) {
		if (m.role.empty())
			throw std::runtime_error("DeepSeekClient: message role cannot be empty");
		if (m.content.empty())
			throw std::runtime_error("DeepSeekClient: message content cannot be empty");
	}
}

void DeepSeekClient::_ensureKeyPresent(const std::string& key) {
	if (key.empty())
		throw std::runtime_error("DeepSeekClient: API key is missing (set DEEPSEEK_API_KEY or provide explicitly)");
}

std::string DeepSeekClient::_buildPayload(const std::vector<Message>& messages,
                                          const Options& opt) {
	std::ostringstream oss;
	oss << "{";
	oss << "\"model\":\"" << jsonEscape(opt.model) << "\",";
	oss << "\"messages\":[";
	for (size_t i = 0; i < messages.size(); ++i) {
		const auto &m = messages[i];
		oss << "{\"role\":\"" << jsonEscape(m.role) << "\","
		    << "\"content\":\"" << jsonEscape(m.content) << "\"}";
		if (i + 1 < messages.size()) oss << ",";
	}
	oss << "],";
	oss << "\"temperature\":" << opt.temperature << ",";
	oss << "\"top_p\":" << opt.top_p << ",";
	oss << "\"max_tokens\":" << opt.max_tokens;
	if (opt.stream) {
		oss << ",\"stream\":true";
	}
	oss << "}";
	return oss.str();
}

#if DEEPSEEK_HAVE_CURL
// libcurl write callback (collect full response body)
static size_t writeToString(void *ptr, size_t size, size_t nmemb, void *userdata) {
	auto *out = static_cast<std::string*>(userdata);
	size_t total = size * nmemb;
	out->append(static_cast<const char*>(ptr), total);
	return total;
}
#endif

std::string DeepSeekClient::_httpPost(const std::string& url,
                                      const std::string& jsonPayload,
                                      const Options& opt,
                                      const std::string& apiKey) {
#if !DEEPSEEK_HAVE_CURL
	// -------- Exec curl fallback (no libcurl) ----------
	// We delegate HTTPS + TLS handling to external 'curl' binary.
	// Pros: rapid implementation, full TLS support if curl is installed.
	// Cons: fork/exec overhead; API key visible in process list (security risk).
	(void)opt;
	if (apiKey.empty())
		throw std::runtime_error("DeepSeekClient::_httpPost (fallback): missing API key");

	// Build JSON payload via stdin (safer than putting JSON on command line).

	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0)
		throw std::runtime_error("DeepSeekClient::_httpPost: pipe creation failed");

	pid_t pid = fork();
	if (pid < 0) {
		close(stdinPipe[0]); close(stdinPipe[1]);
		close(stdoutPipe[0]); close(stdoutPipe[1]);
		throw std::runtime_error("DeepSeekClient::_httpPost: fork failed");
	}

	if (pid == 0) {
		// Child: setup stdin/stdout, exec curl
		close(stdinPipe[1]); // child reads from stdinPipe[0]
		close(stdoutPipe[0]); // child writes to stdoutPipe[1]
		if (dup2(stdinPipe[0], STDIN_FILENO) < 0) _exit(127);
		if (dup2(stdoutPipe[1], STDOUT_FILENO) < 0) _exit(127);
		if (dup2(stdoutPipe[1], STDERR_FILENO) < 0) _exit(127);
		close(stdinPipe[0]);
		close(stdoutPipe[1]);
		// Prepare arguments
		std::string authHeader = "Authorization: Bearer " + apiKey;
		std::vector<char*> argv;
		argv.push_back(const_cast<char*>("curl"));
		argv.push_back(const_cast<char*>("-s"));          // silent
		argv.push_back(const_cast<char*>("-S"));          // show errors
		argv.push_back(const_cast<char*>("--fail"));      // fail on HTTP >=400
		argv.push_back(const_cast<char*>("-X"));
		argv.push_back(const_cast<char*>("POST"));
		argv.push_back(const_cast<char*>("-H"));
		argv.push_back(const_cast<char*>("Content-Type: application/json"));
		argv.push_back(const_cast<char*>("-H"));
		argv.push_back(const_cast<char*>(authHeader.c_str()));
		argv.push_back(const_cast<char*>("--data-binary"));
		argv.push_back(const_cast<char*>("@-"));          // read body from stdin
		argv.push_back(const_cast<char*>(url.c_str()));
		argv.push_back(nullptr);
		execvp("curl", argv.data());
		_exit(127);
	}

	// Parent
	close(stdinPipe[0]);
	close(stdoutPipe[1]);
	// Write payload to child's stdin
	size_t written = 0;
	while (written < jsonPayload.size()) {
		ssize_t w = write(stdinPipe[1], jsonPayload.data() + written, jsonPayload.size() - written);
		if (w < 0) {
			if (errno == EINTR) continue;
			close(stdinPipe[1]);
			close(stdoutPipe[0]);
			throw std::runtime_error("DeepSeekClient::_httpPost: write payload failed");
		}
		written += static_cast<size_t>(w);
	}
	close(stdinPipe[1]);

	// Read all output
	std::string output;
	char buf[4096];
	for (;;) {
		ssize_t r = read(stdoutPipe[0], buf, sizeof(buf));
		if (r == 0) break;
		if (r < 0) {
			if (errno == EINTR) continue;
			break;
		}
		output.append(buf, r);
	}
	close(stdoutPipe[0]);
	int status = 0;
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		throw std::runtime_error("DeepSeekClient::_httpPost: curl exited with error, raw output: " + output);
	}
	// curl outputs only the body by default (-s -S --fail) unless HTTP error occurs.
	return output;

	// (Legacy raw-socket HTTP code removed – exec curl fallback in use.)

	// (Removed obsolete raw-socket remnants)

// (Removed unreachable legacy response buffering block)

	// (Socket handling not applicable with exec curl path)

	// (Legacy response parse removed)
#else
	CURL *curl = curl_easy_init();
	if (!curl)
		throw std::runtime_error("DeepSeekClient::_httpPost: curl_easy_init failed");

	std::string response;
	struct curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	std::string auth = "Authorization: Bearer " + apiKey;
	headers = curl_slist_append(headers, auth.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)jsonPayload.size());
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt.timeoutSeconds);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	CURLcode rc = curl_easy_perform(curl);
	long httpCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) {
		throw std::runtime_error(std::string("DeepSeekClient::_httpPost: curl perform error: ") +
		                         curl_easy_strerror(rc));
	}
	if (httpCode < 200 || httpCode >= 300) {
		std::ostringstream oss;
		oss << "DeepSeekClient::_httpPost: HTTP " << httpCode << " body: " << response;
		throw std::runtime_error(oss.str());
	}
	return response;
#endif
}

// Streaming: buffer data and split lines; SSE style "data: {json}" lines.
void DeepSeekClient::_httpPostStream(const std::string& url,
                                     const std::string& jsonPayload,
                                     const Options& opt,
                                     const std::string& apiKey,
                                     StreamCallback cb) {
#if !DEEPSEEK_HAVE_CURL
	// Basic non-chunked fallback: perform a single blocking POST (same as _httpPost),
	// then simulate a single "chunk" callback with entire response content.
	std::string body = _httpPost(url, jsonPayload, opt, apiKey);
	// Attempt to split by lines beginning with "data:"
	// If body already contains SSE style lines, reuse; else treat as one chunk.
	bool hasDataLine = body.find("\ndata:") != std::string::npos || body.rfind("data:", 0) == 0;
	if (hasDataLine) {
		std::istringstream iss(body);
		std::string line;
		while (std::getline(iss, line)) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			_dispatchStreamPayload(line, cb);
		}
	} else {
		ResponseChunk chunk;
		chunk.content = body;
		chunk.done = false;
		cb(chunk);
	}
	ResponseChunk finalChunk;
	finalChunk.done = true;
	cb(finalChunk);
#else
	CURL *curl = curl_easy_init();
	if (!curl)
		throw std::runtime_error("DeepSeekClient::_httpPostStream: curl_easy_init failed");

	struct StreamCtx {
		std::string buffer;
		StreamCallback cb;
	};

	StreamCtx ctx{ "", cb };

	struct curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	std::string auth = "Authorization: Bearer " + apiKey;
	headers = curl_slist_append(headers, auth.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)jsonPayload.size());
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt.timeoutSeconds);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
		+[](void *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
			auto *c = static_cast<StreamCtx*>(userdata);
			size_t total = size * nmemb;
			c->buffer.append(static_cast<const char*>(ptr), total);

			// Process line by line
			size_t pos;
			while ((pos = c->buffer.find('\n')) != std::string::npos) {
				std::string line = c->buffer.substr(0, pos);
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				c->buffer.erase(0, pos + 1);
				if (!line.empty())
					DeepSeekClient::_dispatchStreamPayload(line, c->cb);
			}
			return total;
		});
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

	CURLcode rc = curl_easy_perform(curl);
	long httpCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) {
		throw std::runtime_error(std::string("DeepSeekClient::_httpPostStream: curl perform error: ") +
		                         curl_easy_strerror(rc));
	}
	if (httpCode < 200 || httpCode >= 300) {
		throw std::runtime_error("DeepSeekClient::_httpPostStream: HTTP status " + std::to_string(httpCode));
	}

	// Signal end if not already
	DeepSeekClient::ResponseChunk finalChunk;
	finalChunk.done = true;
	cb(finalChunk);
#endif
}

std::string DeepSeekClient::_extractContent(const std::string& rawResponse,
                                            const Options& /*opt*/) {
	std::string content;
	if (extractAssistantContent(rawResponse, content))
		return content;

	// Fallback: try first occurrence of "content"
	if (extractFirstJsonStringValue(rawResponse, "content", content))
		return content;

	// If still nothing, return raw body (last resort)
	return rawResponse;
}

void DeepSeekClient::_dispatchStreamPayload(const std::string& line,
                                            StreamCallback cb) {
	std::string trimmed = trim(line);
	if (trimmed.empty()) return;

	// Expect lines like: data: {...}   or  data: [DONE]
	if (trimmed.rfind("data:", 0) != 0)
		return;

	std::string data = trim(trimmed.substr(5));
	if (data == "[DONE]") {
		ResponseChunk chunk;
		chunk.done = true;
		cb(chunk);
		return;
	}

	// Attempt to extract "content":"..." from the JSON fragment.
	std::string content;
	if (extractFirstJsonStringValue(data, "content", content)) {
		ResponseChunk chunk;
		chunk.content = content;
		chunk.done = false;
		cb(chunk);
	}
}

std::string DeepSeekClient::complete(const std::vector<Message>& messages,
                                     const Options& opt) {
	_validateMessages(messages);
	_ensureKeyPresent(_apiKey);

	Options localOpt = opt;
	localOpt.stream = false; // force non-streaming here
	std::string payload = _buildPayload(messages, localOpt);

	// Compose endpoint
	std::string url = localOpt.apiBase + "/v1/chat/completions";

	std::string response = _httpPost(url, payload, localOpt, _apiKey);
	return _extractContent(response, localOpt);
}

void DeepSeekClient::completeStream(const std::vector<Message>& messages,
                                    const Options& opt,
                                    StreamCallback onChunk) {
	if (!onChunk)
		throw std::runtime_error("DeepSeekClient::completeStream: onChunk callback is null");
	_validateMessages(messages);
	_ensureKeyPresent(_apiKey);

	Options localOpt = opt;
	localOpt.stream = true;
	std::string payload = _buildPayload(messages, localOpt);

	std::string url = localOpt.apiBase + "/v1/chat/completions";

	_httpPostStream(url, payload, localOpt, _apiKey, onChunk);
}
