#include "AgentNode.h"
#include "AgentPersistence.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &AgentNode::GetPluginInformation;
}
#endif

AgentNode::AgentNode(Model* model, std::string name)
	: GraphNode(model, name, Util::TypeOf<AgentNode>()) {
	auto* behavior = new SimulationControlString(
		[this]() { return _behavior != nullptr ? _behavior->getName() : std::string(""); },
		[this](std::string name) { setBehavior(FindBehavior(_parentModel, name)); },
		Util::TypeOf<AgentNode>(), getName(), "Behavior");
	_parentModel->getControls()->insert(behavior);
	_addSimulationControl(behavior);
	// The world model belongs to the agent, so its properties are shown inline and it cannot be replaced here.
	auto* worldModel = new SimulationControlGenericClassNotDC<ModelDataDefinition*, Model*, ModelDataDefinition>(
		_parentModel, [this]() -> ModelDataDefinition* { return _worldModel; }, nullptr,
		Util::TypeOf<AgentNode>(), getName(), "WorldModel");
	_parentModel->getControls()->insert(worldModel);
	_addSimulationControl(worldModel);
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
	// The behavior is another element of the model. Until it is loaded, the loader tries this agent again.
	const std::string behavior = fields->loadField("behavior", std::string(""));
	if (!behavior.empty() && FindBehavior(model, behavior) == nullptr) {
		return nullptr;
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

AgentBehavior* AgentNode::FindBehavior(Model* model, const std::string& name) {
	for (AgentBehavior* behavior : model->getDataManager()->getDataDefinitionsAssignableTo<AgentBehavior>()) {
		if (behavior->getName() == name) {
			return behavior;
		}
	}
	return nullptr;
}

bool AgentNode::_loadInstance(PersistenceRecord* fields) {
	bool res = GraphNode::_loadInstance(fields);
	if (res) {
		setBehavior(FindBehavior(_parentModel, fields->loadField("behavior", std::string(""))));
		setWorldModel(dynamic_cast<AgentWorldModel*>(LoadNested(_parentModel, fields, "worldModel.")));
	}
	return res;
}

void AgentNode::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	GraphNode::_saveInstance(fields, saveDefaultValues);
	fields->saveField("behavior", _behavior != nullptr ? _behavior->getName() : std::string(""));
	if (_worldModel != nullptr) {
		SaveNested(fields, "worldModel.", _worldModel);
	}
}
