#ifndef AGENTNODE_H
#define AGENTNODE_H

#include "AgentBehavior.h"
#include "AgentWorldModel.h"
#include "plugins/data/ModalModel/GraphNode.h"

/*!
 * \brief Agent of an AgentNetwork: a graph node with a reference to the behavior of its type and
 * its own world model, which it owns.
 */
class AgentNode : public GraphNode {
public:
	AgentNode(Model* model, std::string name = "");
	virtual ~AgentNode() override;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	void setBehavior(AgentBehavior* behavior);
	AgentBehavior* getBehavior() const;
	/*! \brief The agent takes ownership of \p worldModel. */
	void setWorldModel(AgentWorldModel* worldModel);
	AgentWorldModel* getWorldModel() const;
	/*! \brief Finds the behavior named \p name among the behaviors of \p model, of any subclass. */
	static AgentBehavior* FindBehavior(Model* model, const std::string& name);

protected:
	virtual bool _loadInstance(PersistenceRecord* fields) override;
	virtual void _saveInstance(PersistenceRecord* fields, bool saveDefaultValues) override;

private:
	AgentBehavior* _behavior = nullptr;
	AgentWorldModel* _worldModel = nullptr;
};

#endif /* AGENTNODE_H */
