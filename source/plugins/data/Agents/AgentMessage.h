#ifndef AGENTMESSAGE_H
#define AGENTMESSAGE_H

#include <string>
#include <utility>

class AgentNode;

/*!
 * \brief Message from an agent to another agent or, when the receiver is null, to the environment.
 *
 * Each kind of message is a subclass that defines its content and identifies itself by its
 * type name, which behaviors, channels and environments use to declare what they produce and accept.
 */
class AgentMessage {
public:
	AgentMessage(std::string type, AgentNode* sender, AgentNode* receiver)
		: _type(std::move(type)), _sender(sender), _receiver(receiver) {}
	virtual ~AgentMessage() = default;

public:
	const std::string& getType() const { return _type; }
	AgentNode* getSender() const { return _sender; }
	AgentNode* getReceiver() const { return _receiver; }
	/*! \brief Content written to the round trace. */
	virtual std::string show() const = 0;

private:
	std::string _type;
	AgentNode* _sender;
	AgentNode* _receiver;
};

#endif /* AGENTMESSAGE_H */
