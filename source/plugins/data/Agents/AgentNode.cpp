#include "AgentNode.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &AgentNode::GetPluginInformation;
}
#endif

AgentNode::AgentNode(Model* model, std::string name)
	: GraphNode(model, name, Util::TypeOf<AgentNode>()) {
}

AgentNode::~AgentNode() {
	delete _worldModel;
}

PluginInformation* AgentNode::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<AgentNode>(), &AgentNode::LoadInstance, &AgentNode::NewInstance);
	info->setCategory("Agents");
	info->setDataDefinitionBaseTypeName(Util::TypeOf<GraphNode>());
	info->setDescriptionHelp("Agent of an AgentNetwork, with a behavior and its own world model.");
	return info;
}

ModelDataDefinition* AgentNode::LoadInstance(Model* model, PersistenceRecord* fields) {
	// Reuse the agent already loaded from the network record or from its own record, as GraphNode does.
	ModelDataDefinition* existing = model->getDataManager()->getDataDefinition(Util::TypeOf<AgentNode>(), fields->loadField("name", std::string("")));
	if (existing != nullptr) {
		return existing;
	}
	AgentNode* agent = new AgentNode(model);
	agent->_loadInstance(fields);
	return agent;
}

ModelDataDefinition* AgentNode::NewInstance(Model* model, std::string name) {
	return new AgentNode(model, name);
}

void AgentNode::setBehavior(AgentBehavior* behavior) {
	_behavior = behavior;
	if (behavior != nullptr) {
		_attachedDataInsert("Behavior", behavior);
	} else {
		_attachedDataRemove("Behavior");
	}
}

AgentBehavior* AgentNode::getBehavior() const {
	return _behavior;
}

void AgentNode::setWorldModel(AgentWorldModel* worldModel) {
	if (worldModel != _worldModel) {
		delete _worldModel;
		_worldModel = worldModel;
	}
}

AgentWorldModel* AgentNode::getWorldModel() const {
	return _worldModel;
}
