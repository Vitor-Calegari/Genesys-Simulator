#ifndef AGENTCHANNEL_H
#define AGENTCHANNEL_H

#include "plugins/data/ModalModel/GraphEdge.h"

#include <string>
#include <vector>

/*!
 * \brief Directed link that lets the source agent send messages of the accepted types to the
 * destination agent. Delivery belongs to the MessageBroker, so the channel holds no messages.
 */
class AgentChannel : public GraphEdge {
public:
	AgentChannel(Model* model, std::string name = "");
	virtual ~AgentChannel() override = default;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	void setAcceptedMessageTypes(const std::vector<std::string>& types);
	const std::vector<std::string>& acceptedMessageTypes() const;
	bool accepts(const std::string& type) const;

private:
	std::vector<std::string> _acceptedMessageTypes;
};

#endif /* AGENTCHANNEL_H */
