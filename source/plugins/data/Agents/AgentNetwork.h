#ifndef AGENTNETWORK_H
#define AGENTNETWORK_H

#include "AgentChannel.h"
#include "AgentEnvironment.h"
#include "AgentNode.h"
#include "MessageBroker.h"
#include "plugins/data/ModalModel/DirectedGraphNetwork.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

/*!
 * \brief Network of agents activated by a ModalModel: each activation runs one synchronous round.
 *
 * In a round every agent perceives and decides on the state left by the previous round, in the
 * order of the agent keys, and only then the valid decisions are applied and their messages are
 * delivered to the MessageBroker and to the environment, which is optional and owned by the network.
 */
class AgentNetwork : public DirectedGraphNetwork {
public:
	AgentNetwork(Model* model, std::string name = "");
	virtual ~AgentNetwork() override;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	virtual bool addNode(GraphNode* node) override;
	virtual bool addEdge(GraphEdge* edge) override;
	/*! \brief The network takes ownership of \p environment. */
	void setEnvironment(AgentEnvironment* environment);
	AgentEnvironment* getEnvironment() const;
	unsigned int getRound() const;

protected:
	virtual void _initBetweenReplications() override;
	virtual NetworkActivationResult _activate(const NetworkActivationFrame& frame) override;

private:
	std::vector<AgentNode*> _agentsByKey() const;
	std::map<AgentNode*, std::vector<AgentChannel*>> _channelsBySource() const;
	bool _accepts(AgentNode* sender, const std::vector<AgentChannel*>& channels, const AgentMessage* message) const;
	void _traceRound(AgentNode* agent, const AgentDecision* decision, const std::string& failure);

private:
	AgentEnvironment* _environment = nullptr;
	MessageBroker _broker;
	unsigned int _round = 0;
};

#endif /* AGENTNETWORK_H */
