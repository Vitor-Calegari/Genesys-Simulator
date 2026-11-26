#ifndef AGENTBEHAVIOR_H
#define AGENTBEHAVIOR_H

#include "AgentDecision.h"
#include "AgentWorldModel.h"

#include <memory>
#include <string>
#include <vector>

/*! \brief What an agent perceives at the start of a round. */
struct AgentPerception {
	AgentNode* agent = nullptr;
	/*! \brief Destinations of the agent's outgoing channels. */
	std::vector<AgentNode*> neighbors;
	/*! \brief Messages delivered by the MessageBroker. */
	std::vector<std::shared_ptr<AgentMessage>> messages;
	/*! \brief What the agent perceives of the environment. */
	std::vector<std::shared_ptr<AgentMessage>> environment;
};

/*!
 * \brief How one type of agent decides and how a decision changes its world model.
 *
 * A behavior is shared by every agent that references it. decide() reads the perception and the
 * world model and must not change any state, and throws when the decision fails. apply() runs
 * only after every agent has decided, and only for valid decisions. endRound() closes each round.
 */
class AgentBehavior : public ModelDataDefinition {
public:
	AgentBehavior(Model* model, std::string typeName, std::string name = "")
		: ModelDataDefinition(model, typeName, name) {}

public:
	virtual std::unique_ptr<AgentDecision> decide(const AgentPerception& perception, const AgentWorldModel* worldModel) = 0;
	virtual void apply(AgentDecision* decision, AgentWorldModel* worldModel) = 0;
	virtual std::string worldModelType() const = 0;
	virtual std::vector<std::string> producedMessageTypes() const = 0;
	virtual std::vector<std::string> acceptedMessageTypes() const = 0;
	/*! \brief Records the measures of a round over the agents of this behavior, after the decisions are applied. */
	virtual void endRound(const std::vector<AgentNode*>& /*agents*/) {}
};

#endif /* AGENTBEHAVIOR_H */
