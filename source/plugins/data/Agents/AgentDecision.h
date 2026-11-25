#ifndef AGENTDECISION_H
#define AGENTDECISION_H

#include "AgentMessage.h"

#include <memory>
#include <string>
#include <vector>

/*!
 * \brief Decision of one agent in one round: the messages to send and, in subclasses,
 * whatever the behavior needs in apply() to update the world model.
 */
class AgentDecision {
public:
	virtual ~AgentDecision() = default;
	/*! \brief Text written to the round trace, besides the messages. */
	virtual std::string show() const { return ""; }

public:
	std::vector<std::shared_ptr<AgentMessage>> messages;
};

#endif /* AGENTDECISION_H */
