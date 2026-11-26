#include "LLMWorldModel.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#include <functional>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &LLMWorldModel::GetPluginInformation;
}
#endif

LLMWorldModel::LLMWorldModel(Model* model, std::string name)
	: AgentWorldModel(model, Util::TypeOf<LLMWorldModel>(), name) {
	auto* persona = new SimulationControlString(
		std::bind(&LLMWorldModel::getPersona, this), std::bind(&LLMWorldModel::setPersona, this, std::placeholders::_1),
		Util::TypeOf<LLMWorldModel>(), getName(), "Persona", "Who the agent is, written in the prompt of each of its decisions.");
	persona->setPreferredEditorHint(SimulationControlEditorHint::MultiLineText);
	_parentModel->getControls()->insert(persona);
	_addSimulationControl(persona);
}

PluginInformation* LLMWorldModel::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<LLMWorldModel>(), &LLMWorldModel::LoadInstance, &LLMWorldModel::NewInstance);
	info->setCategory("Agents");
	info->setDescriptionHelp("World model of an LLM agent, with its persona and the memory of the private messages it received and what it decided.");
	return info;
}

ModelDataDefinition* LLMWorldModel::LoadInstance(Model* model, PersistenceRecord* fields) {
	LLMWorldModel* worldModel = new LLMWorldModel(model);
	worldModel->_loadInstance(fields);
	return worldModel;
}

ModelDataDefinition* LLMWorldModel::NewInstance(Model* model, std::string name) {
	return new LLMWorldModel(model, name);
}

void LLMWorldModel::setPersona(const std::string& persona) {
	_persona = persona;
}

std::string LLMWorldModel::getPersona() const {
	return _persona;
}

std::string LLMWorldModel::getMemory() const {
	return _memory;
}

void LLMWorldModel::remember(const std::string& text) {
	_memory += text;
}

void LLMWorldModel::reset() {
	_memory.clear();
}

std::string LLMWorldModel::show() {
	return _memory;
}

bool LLMWorldModel::_loadInstance(PersistenceRecord* fields) {
	bool res = AgentWorldModel::_loadInstance(fields);
	if (res) {
		_persona = fields->loadField("persona", std::string(""));
	}
	return res;
}

void LLMWorldModel::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	AgentWorldModel::_saveInstance(fields, saveDefaultValues);
	fields->saveField("persona", _persona, std::string(""), saveDefaultValues);
}
