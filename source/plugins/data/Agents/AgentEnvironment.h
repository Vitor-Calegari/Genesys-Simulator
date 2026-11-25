#ifndef AGENTENVIRONMENT_H
#define AGENTENVIRONMENT_H

#include "AgentMessage.h"
#include "kernel/simulator/model/ModelDataDefinition.h"

#include <memory>
#include <string>
#include <vector>

/*!
 * \brief Environment shared by the agents of a network, perceived by every agent and changed
 * by the messages sent to it.
 *
 * The environment belongs to its network and is saved inside the network record with the type
 * of its subclass. A subclass clears its state between replications in _initBetweenReplications().
 */
class AgentEnvironment : public ModelDataDefinition {
public:
	AgentEnvironment(Model* model, std::string typeName, std::string name = "")
		: ModelDataDefinition(model, typeName, name, false) {}

public:
	virtual std::vector<std::shared_ptr<AgentMessage>> perceive(AgentNode* agent) const = 0;
	/*! \brief Receives the messages of a round ordered by sender key, and resolves conflicts between them. */
	virtual void apply(const std::vector<std::shared_ptr<AgentMessage>>& messages) = 0;
	virtual std::vector<std::string> acceptedMessageTypes() const = 0;
};

#endif /* AGENTENVIRONMENT_H */
