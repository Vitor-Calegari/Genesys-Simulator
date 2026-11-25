#include "AgentChannel.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#include <algorithm>
#include <sstream>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &AgentChannel::GetPluginInformation;
}
#endif

namespace {
// Message types are written separated by spaces, and read separated by spaces or commas.
std::string join(const std::vector<std::string>& types) {
	std::string text;
	for (const std::string& type : types) {
		text += (text.empty() ? "" : " ") + type;
	}
	return text;
}

std::vector<std::string> split(std::string text) {
	std::replace(text.begin(), text.end(), ',', ' ');
	std::istringstream stream(text);
	std::vector<std::string> types;
	for (std::string type; stream >> type;) {
		types.push_back(type);
	}
	return types;
}
}

AgentChannel::AgentChannel(Model* model, std::string name)
	: GraphEdge(model, name, Util::TypeOf<AgentChannel>()) {
	setDirected(true);
}

PluginInformation* AgentChannel::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<AgentChannel>(), &AgentChannel::LoadInstance, &AgentChannel::NewInstance);
	info->setCategory("Agents");
	info->setDataDefinitionBaseTypeName(Util::TypeOf<GraphEdge>());
	info->setDescriptionHelp("Directed channel between two agents of an AgentNetwork, with the message types it accepts.");
	return info;
}

ModelDataDefinition* AgentChannel::LoadInstance(Model* model, PersistenceRecord* fields) {
	// Reuse the channel already loaded from the network record or from its own record, as GraphEdge does.
	ModelDataDefinition* existing = model->getDataManager()->getDataDefinition(Util::TypeOf<AgentChannel>(), fields->loadField("name", std::string("")));
	if (existing != nullptr) {
		return existing;
	}
	AgentChannel* channel = new AgentChannel(model);
	channel->_loadInstance(fields);
	return channel;
}

ModelDataDefinition* AgentChannel::NewInstance(Model* model, std::string name) {
	return new AgentChannel(model, name);
}

void AgentChannel::setAcceptedMessageTypes(const std::vector<std::string>& types) {
	_acceptedMessageTypes = types;
}

const std::vector<std::string>& AgentChannel::acceptedMessageTypes() const {
	return _acceptedMessageTypes;
}

bool AgentChannel::accepts(const std::string& type) const {
	return std::find(_acceptedMessageTypes.begin(), _acceptedMessageTypes.end(), type) != _acceptedMessageTypes.end();
}

bool AgentChannel::_loadInstance(PersistenceRecord* fields) {
	bool res = GraphEdge::_loadInstance(fields);
	if (res) {
		_acceptedMessageTypes = split(fields->loadField("acceptedMessageTypes", std::string("")));
	}
	return res;
}

void AgentChannel::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	GraphEdge::_saveInstance(fields, saveDefaultValues);
	fields->saveField("acceptedMessageTypes", join(_acceptedMessageTypes), std::string(""), saveDefaultValues);
}
