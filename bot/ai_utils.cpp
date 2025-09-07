#include <iostream>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>

const std::string HOST = "api.deepseek.com";
const std::string API_KEY = "sk-1ae611193c1a48e9b12119aa587e4801";
const std::string PATH = "/v1/chat/completions";  // Correct DeepSeek chat endpoint

// Simple HTTP request with raw sockets
int send_http_request(const std::string& host, const std::string& path, const std::string& payload) {
    // Minimal fix: use system curl for HTTPS instead of raw insecure port 80 socket.
    // NOTE: This exposes the API key in the process list. Do NOT use this approach in production.
    std::ostringstream cmd;
    cmd << "curl -sS -X POST https://" << host << path
        << " -H 'Authorization: Bearer " << API_KEY << "'"
        << " -H 'Content-Type: application/json'"
        << " -H 'Accept: application/json'"
        << " --fail"
        << " -d '" << payload << "'";
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: failed to run curl" << std::endl;
        return -1;
    }
    std::string response;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        response.append(buf);
    }
    int rc = pclose(pipe);
    if (rc != 0) {
        std::cerr << "curl exited with code " << rc << "\nRaw/partial response:\n" << response << std::endl;
        return -1;
    }

    // Naive JSON extraction of the first assistant message "content"
    std::string extracted;
    // Prefer content that follows an assistant role, fallback to first content field
    size_t searchStart = response.find("\"role\":\"assistant\"");
    if (searchStart == std::string::npos)
        searchStart = 0;
    size_t keyPos = response.find("\"content\"", searchStart);
    if (keyPos != std::string::npos) {
        size_t colon = response.find(':', keyPos);
        if (colon != std::string::npos) {
            size_t firstQuote = response.find('"', colon);
            if (firstQuote != std::string::npos) {
                size_t i = firstQuote + 1;
                while (i < response.size()) {
                    char c = response[i];
                    if (c == '\\' && i + 1 < response.size()) {
                        // Handle simple escape sequences
                        char next = response[i + 1];
                        if (next == 'n') {
                            extracted.push_back('\n');
                        } else if (next == 'r') {
                            // skip carriage returns in output
                        } else {
                            extracted.push_back(next);
                        }
                        i += 2;
                        continue;
                    }
                    if (c == '"') {
                        break;
                    }
                    extracted.push_back(c);
                    ++i;
                }
            }
        }
    }

    if (extracted.empty()) {
        // Fallback: print raw if parsing failed
        std::cout << response << std::endl;
    } else {
        std::cout << extracted << std::endl;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        std::cerr << "Usage: " << (argc > 0 ? argv[0] : "program")
                  << " \"your message\"\n";
        return 1;
    }

    // JSON escape (minimal)
    auto jsonEscape = [](const std::string &in) -> std::string {
        std::string out;
        out.reserve(in.size() + 16);
        for (char c : in) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '\"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': /* drop */ break;
                case '\t': out += "\\t"; break;
                default:   out += c; break;
            }
        }
        return out;
    };

    std::string userInput = jsonEscape(argv[1]);

    // Build JSON payload
    std::string json = std::string("{\"model\":\"deepseek-chat\",\"messages\":[{\"role\":\"user\",\"content\":\"")
        + userInput + "\"}],\"temperature\":0.7}";

    // Because send_http_request embeds the payload inside single quotes for curl,
    // we must shell-escape single quotes to avoid breaking the command.
    std::string shellEscaped;
    shellEscaped.reserve(json.size() + 32);
    for (char c : json) {
        if (c == '\'')
            shellEscaped += "'\"'\"'"; // close ' add " ' " reopen '
        else
            shellEscaped += c;
    }

    send_http_request(HOST, PATH, shellEscaped);
    return 0;
}
