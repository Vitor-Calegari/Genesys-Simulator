#include "MessageBroker.h"

void MessageBroker::send(const std::shared_ptr<AgentMessage>& message) {
	_sent[message->getReceiver()].push_back(message);
}

std::vector<std::shared_ptr<AgentMessage>> MessageBroker::inbox(AgentNode* agent) const {
	auto it = _readable.find(agent);
	return it != _readable.end() ? it->second : std::vector<std::shared_ptr<AgentMessage>>();
}

void MessageBroker::redeliver(AgentNode* agent) {
	std::vector<std::shared_ptr<AgentMessage>>& sent = _sent[agent];
	std::vector<std::shared_ptr<AgentMessage>> readable = inbox(agent);
	sent.insert(sent.begin(), readable.begin(), readable.end());
}

void MessageBroker::endRound() {
	_readable = std::move(_sent);
	_sent.clear();
}

void MessageBroker::clear() {
	_readable.clear();
	_sent.clear();
}
