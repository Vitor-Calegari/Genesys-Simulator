#ifndef SIMULATEDPROVIDERCLIENTIMPL_H
#define SIMULATEDPROVIDERCLIENTIMPL_H

#include "AIProviderClient_if.h"

#include <map>
#include <string>
#include <vector>

/**
 * @brief Provider that answers with predefined responses, without an API key or network.
 *
 * @details
 * The responses are written one per line as "agent: response". The agent of a request is the
 * name in the first line of its prompt, written as "Agent: name". The n-th call of an agent
 * receives the n-th response of that agent, and an agent with no response left does not act.
 * Tokens are counted as one per four characters of text.
 */
class SimulatedProviderClientImpl : public AIProviderClient_if {
public:
    explicit SimulatedProviderClientImpl(const std::string& responses);

    std::string getProviderName() const override { return "Simulated"; }
    bool isAvailable() const override { return true; }
    void setBaseUrl(const std::string& url) override { _baseUrl = url; }
    std::string getBaseUrl() const override { return _baseUrl; }
    void setTimeoutSeconds(unsigned int seconds) override { _timeoutSeconds = seconds; }
    unsigned int getTimeoutSeconds() const override { return _timeoutSeconds; }
    void setApiKey(const std::string&) override {}
    void clearApiKey() override {}
    bool hasApiKey() const override { return false; }
    ProviderResponse send(const ProviderRequest& request) override;

private:
    std::map<std::string, std::vector<std::string>> _responses;
    std::map<std::string, std::size_t> _calls;
    std::string _baseUrl;
    unsigned int _timeoutSeconds = 0;
};

#endif /* SIMULATEDPROVIDERCLIENTIMPL_H */
