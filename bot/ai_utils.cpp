#include <iostream>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <stdexcept>

const std::string HOST = "api.deepseek.com";
const std::string API_KEY = "...";
const std::string PATH = "/v1/chat/completions";

std::string jsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string send_http_request(const std::string& host, const std::string& path, const std::string& payload) {
    std::ostringstream cmd;
    cmd << "curl -sS -X POST https://" << host << path
        << " -H 'Authorization: Bearer " << API_KEY << "'"
        << " -H 'Content-Type: application/json'"
        << " -H 'Accept: application/json'"
        << " --fail"
        << " -d '" << payload << "'";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Error: failed to run curl");
    }

    std::string response;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        response.append(buf);
    }

    int rc = pclose(pipe);
    if (rc != 0) {
        std::ostringstream error_msg;
        error_msg << "curl exited with code " << rc << "\nRaw/partial response:\n" << response;
        throw std::runtime_error(error_msg.str());
    }

    return response;
}

std::string parse_ai_response(const std::string& response) {
    std::string extracted;
    size_t searchStart = response.find("\"role\":\"assistant\"");
    if (searchStart == std::string::npos) {
        searchStart = 0;
    }

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
                        char next = response[i + 1];
                        if (next == 'n') {
                            extracted.push_back('\n');
                        } else if (next == 'r') {
                            // Skip carriage returns
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

    return extracted.empty() ? response : extracted;
}

std::string talk_to_ai(const std::string& message) {
    std::string userInput = jsonEscape(message);
    const std::string systemPrompt = "You are a concise chatbot for irc server, that will be briefly talking to the user as a friend, be lazy and not always response right. Answer briefly. Answer in less then 500 chars. The output that you  send will be shown in the terminal";
    std::string json =
        "{\"model\":\"deepseek-chat\","
        "\"messages\":["
            "{\"role\":\"system\",\"content\":\"" + jsonEscape(systemPrompt) + "\"},"
            "{\"role\":\"user\",\"content\":\"" + userInput + "\"}"
        "],"
        "\"temperature\":0.7}";
    std::string shellEscaped;
    shellEscaped.reserve(json.size() + 32);
    for (char c : json) {
        if (c == '\'') {
            shellEscaped += "'\"'\"'";
        } else {
            shellEscaped += c;
        }
    }

    try {
        std::string response = send_http_request(HOST, PATH, shellEscaped);
        return parse_ai_response(response);
    } catch (const std::exception& e) {
        return "Error: " + std::string(e.what());
    }
}
