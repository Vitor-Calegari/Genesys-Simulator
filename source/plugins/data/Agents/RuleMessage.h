#ifndef RULEMESSAGE_H
#define RULEMESSAGE_H

#include "AgentMessage.h"
#include "kernel/util/Util.h"

/*! \brief Notice that a rule-based agent sends to its neighbors when its condition holds. */
class RuleMessage : public AgentMessage {
public:
	RuleMessage(AgentNode* sender, AgentNode* receiver)
		: AgentMessage(Util::TypeOf<RuleMessage>(), sender, receiver) {}
	std::string show() const override { return "notice"; }
};

#endif /* RULEMESSAGE_H */
