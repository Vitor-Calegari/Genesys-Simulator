#include "RuleWorldModel.h"
#include "RuleDecision.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#include <functional>
#include <sstream>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &RuleWorldModel::GetPluginInformation;
}
#endif

RuleWorldModel::RuleWorldModel(Model* model, std::string name)
	: AgentWorldModel(model, Util::TypeOf<RuleWorldModel>(), name) {
	auto* initialValues = new SimulationControlString(
		std::bind(&RuleWorldModel::getInitialValues, this), std::bind(&RuleWorldModel::setInitialValues, this, std::placeholders::_1),
		Util::TypeOf<RuleWorldModel>(), getName(), "InitialValues", "Values of this agent as variable = value; ...");
	_parentModel->getControls()->insert(initialValues);
	_addSimulationControl(initialValues);
}

PluginInformation* RuleWorldModel::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<RuleWorldModel>(), &RuleWorldModel::LoadInstance, &RuleWorldModel::NewInstance);
	info->setCategory("Agents");
	info->setDescriptionHelp("World model of a rule-based agent, with its values for the Variables the rules use.");
	return info;
}

ModelDataDefinition* RuleWorldModel::LoadInstance(Model* model, PersistenceRecord* fields) {
	RuleWorldModel* worldModel = new RuleWorldModel(model);
	worldModel->_loadInstance(fields);
	return worldModel;
}

ModelDataDefinition* RuleWorldModel::NewInstance(Model* model, std::string name) {
	return new RuleWorldModel(model, name);
}

void RuleWorldModel::setInitialValue(const std::string& variable, double value) {
	_initialValues[variable] = value;
	_values[variable] = value;
}

void RuleWorldModel::setInitialValues(const std::string& text) {
	_initialValues.clear();
	std::istringstream clauses(text);
	for (std::string clause; std::getline(clauses, clause, ';');) {
		const std::size_t equals = clause.find('=');
		if (equals != std::string::npos) {
			_initialValues[Util::Trim(clause.substr(0, equals))] = std::stod(clause.substr(equals + 1));
		}
	}
	_values = _initialValues;
}

std::string RuleWorldModel::getInitialValues() const {
	return ShowRuleValues(_initialValues);
}

double RuleWorldModel::getValue(const std::string& variable, double defaultValue) const {
	auto it = _values.find(variable);
	return it != _values.end() ? it->second : defaultValue;
}

void RuleWorldModel::setValue(const std::string& variable, double value) {
	_values[variable] = value;
}

const std::map<std::string, double>& RuleWorldModel::getValues() const {
	return _values;
}

void RuleWorldModel::reset() {
	_values = _initialValues;
}

std::string RuleWorldModel::show() {
	return ShowRuleValues(_values);
}

bool RuleWorldModel::_loadInstance(PersistenceRecord* fields) {
	bool res = AgentWorldModel::_loadInstance(fields);
	if (res) {
		setInitialValues(fields->loadField("initialValues", std::string("")));
	}
	return res;
}

void RuleWorldModel::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	AgentWorldModel::_saveInstance(fields, saveDefaultValues);
	fields->saveField("initialValues", getInitialValues(), std::string(""), saveDefaultValues);
}
