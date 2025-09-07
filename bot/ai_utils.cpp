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
    std::cout << "Response:\n" << response << std::endl;
    return 0;
}

int main() {
    std::string payload = R"({
        "model": "deepseek-chat",
        "messages": [{"role": "user", "content": "Hello, who are you?"}],
        "temperature": 0.7
    })";
    send_http_request(HOST, PATH, payload);

    return 0;
}
