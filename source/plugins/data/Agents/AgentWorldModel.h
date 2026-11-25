#ifndef AGENTWORLDMODEL_H
#define AGENTWORLDMODEL_H

#include "kernel/simulator/model/ModelDataDefinition.h"

/*!
 * \brief What one agent is and knows: an initial configuration, which is saved, and an
 * execution state, which reset() restores from the configuration at each replication.
 *
 * A world model belongs to its agent and is saved inside the agent record with the type of its
 * subclass, so it is not inserted into the model as a separate data definition.
 */
class AgentWorldModel : public ModelDataDefinition {
public:
	AgentWorldModel(Model* model, std::string typeName, std::string name = "")
		: ModelDataDefinition(model, typeName, name, false) {}

public:
	virtual void reset() = 0;
};

#endif /* AGENTWORLDMODEL_H */
