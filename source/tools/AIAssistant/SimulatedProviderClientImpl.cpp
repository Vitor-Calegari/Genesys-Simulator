#include "SimulatedProviderClientImpl.h"

#include <sstream>

namespace {
std::string trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r");
    return first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}
}

SimulatedProviderClientImpl::SimulatedProviderClientImpl(const std::string& responses) {
    std::istringstream lines(responses);
    for (std::string line; std::getline(lines, line);) {
        const std::size_t colon = line.find(':');
        if (colon != std::string::npos) {
            _responses[trim(line.substr(0, colon))].push_back(trim(line.substr(colon + 1)));
        }
    }
}

ProviderResponse SimulatedProviderClientImpl::send(const ProviderRequest& request) {
    const std::string& prompt = request.messages.back().content;
    const std::string prefix = "Agent: ";
    const std::string agent = prompt.compare(0, prefix.size(), prefix) == 0
            ? trim(prompt.substr(prefix.size(), prompt.find('\n') - prefix.size())) : "";
    const std::vector<std::string>& responses = _responses[agent];
    const std::size_t call = _calls[agent]++;
    ProviderResponse response;
    response.success = true;
    response.content = call < responses.size() ? responses[call] : "{\"action\": \"none\"}";
    for (const ChatMessage& message : request.messages) {
        response.inputTokens += message.content.size() / 4;
    }
    response.outputTokens = response.content.size() / 4;
    return response;
}
