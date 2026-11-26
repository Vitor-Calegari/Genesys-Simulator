#include "RuleBehavior.h"
#include "AgentNode.h"
#include "RuleDecision.h"
#include "RuleMessage.h"
#include "RuleWorldModel.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/essentialPlugins/StatisticsCollector.h"
#include "kernel/simulator/model/Model.h"
#include "plugins/data/Logic/Variable.h"

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &RuleBehavior::GetPluginInformation;
}
#endif

namespace {
// Splits "variable = expression; ..." into pairs, as the Set Actions of an EFSM transition.
std::vector<std::pair<std::string, std::string>> parseRules(const std::string& rules) {
	std::vector<std::pair<std::string, std::string>> result;
	std::istringstream clauses(rules);
	for (std::string clause; std::getline(clauses, clause, ';');) {
		if (Util::Trim(clause).empty()) {
			continue;
		}
		const std::size_t equals = clause.find('=');
		result.emplace_back(Util::Trim(clause.substr(0, equals)), equals != std::string::npos ? Util::Trim(clause.substr(equals + 1)) : "");
	}
	return result;
}

std::vector<std::string> splitNames(std::string names) {
	std::replace(names.begin(), names.end(), ',', ' ');
	std::istringstream stream(names);
	std::vector<std::string> result;
	for (std::string name; stream >> name;) {
		result.push_back(name);
	}
	return result;
}
}

RuleBehavior::RuleBehavior(Model* model, std::string name)
	: AgentBehavior(model, Util::TypeOf<RuleBehavior>(), name) {
	auto add = [this](const std::string& property, auto getter, auto setter, const std::string& help) {
		auto* control = new SimulationControlString(getter, setter, Util::TypeOf<RuleBehavior>(), getName(), property, help);
		_parentModel->getControls()->insert(control);
		_addSimulationControl(control);
	};
	add("Condition", std::bind(&RuleBehavior::getCondition, this), std::bind(&RuleBehavior::setCondition, this, std::placeholders::_1),
	    "Logical expression, as the guard of an EFSM transition, that makes the agent send a notice to its neighbors.");
	add("Rules", std::bind(&RuleBehavior::getRules, this), std::bind(&RuleBehavior::setRules, this, std::placeholders::_1),
	    "Assignments variable = expression separated by semicolons, as the Set Actions of an EFSM transition.");
	add("MeasuredVariables", std::bind(&RuleBehavior::getMeasuredVariables, this), std::bind(&RuleBehavior::setMeasuredVariables, this, std::placeholders::_1),
	    "Variables measured at each round, for each agent and on average over the agents.");
	add("PerceptionVariable", std::bind(&RuleBehavior::getPerceptionVariable, this), std::bind(&RuleBehavior::setPerceptionVariable, this, std::placeholders::_1),
	    "Variable that receives the number of notices the agent received.");
}

PluginInformation* RuleBehavior::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<RuleBehavior>(), &RuleBehavior::LoadInstance, &RuleBehavior::NewInstance);
	info->setCategory("Agents");
	info->setDescriptionHelp("Behavior of rule-based agents: rules and a condition evaluated by the GenESyS parser with the Variables of each agent.");
	return info;
}

ModelDataDefinition* RuleBehavior::LoadInstance(Model* model, PersistenceRecord* fields) {
	RuleBehavior* behavior = new RuleBehavior(model);
	behavior->_loadInstance(fields);
	return behavior;
}

ModelDataDefinition* RuleBehavior::NewInstance(Model* model, std::string name) {
	return new RuleBehavior(model, name);
}

std::unique_ptr<AgentDecision> RuleBehavior::decide(const AgentPerception& perception, const AgentWorldModel* worldModel) {
	const auto* world = static_cast<const RuleWorldModel*>(worldModel);
	const std::vector<std::pair<std::string, std::string>> rules = parseRules(_rules);

	// The Variables take the values of the agent: its world model, the initial value of each
	// Variable its rules change and the number of notices it received.
	std::vector<std::pair<Variable*, double>> loaded;
	auto load = [&](const std::string& name, double value) {
		Variable* variable = _findVariable(name);
		if (variable == nullptr) {
			throw std::invalid_argument("unknown Variable \"" + name + "\"");
		}
		loaded.emplace_back(variable, value);
	};
	for (const auto& [name, value] : world->getValues()) {
		load(name, value);
	}
	for (const auto& rule : rules) {
		if (world->getValues().count(rule.first) == 0) {
			Variable* variable = _findVariable(rule.first);
			load(rule.first, variable != nullptr ? variable->getInitialValue() : 0.0);
		}
	}
	if (!_perceptionVariable.empty()) {
		load(_perceptionVariable, static_cast<double>(std::count_if(perception.messages.begin(), perception.messages.end(),
				[](const std::shared_ptr<AgentMessage>& message) { return message->getType() == Util::TypeOf<RuleMessage>(); })));
	}
	std::vector<double> own;
	for (const auto& [variable, value] : loaded) {
		own.push_back(variable->getValue());
		variable->setValue(value);
	}

	// Every expression sees the values of the start of the round, and the results go to the decision.
	auto decision = std::make_unique<RuleDecision>();
	bool success = true;
	std::string error;
	for (const auto& rule : rules) {
		decision->values[rule.first] = _parentModel->parseExpression(rule.second, success, error);
		if (!success) {
			break;
		}
	}
	const bool act = success && (_condition.empty() || _parentModel->parseExpression(_condition, success, error) != 0.0);

	// The Variables get their own values back, so outside the round each keeps its own value.
	for (std::size_t i = loaded.size(); i-- > 0;) {
		loaded[i].first->setValue(own[i]);
	}
	if (!success) {
		throw std::invalid_argument(error);
	}
	if (act) {
		for (AgentNode* neighbor : perception.neighbors) {
			decision->messages.push_back(std::make_shared<RuleMessage>(perception.agent, neighbor));
		}
	}
	return decision;
}

void RuleBehavior::apply(AgentDecision* decision, AgentWorldModel* worldModel) {
	auto* world = static_cast<RuleWorldModel*>(worldModel);
	for (const auto& [name, value] : static_cast<RuleDecision*>(decision)->values) {
		world->setValue(name, value);
	}
}

void RuleBehavior::endRound(const std::vector<AgentNode*>& agents) {
	for (const auto& [name, collectors] : _collectors) {
		const double initial = _findVariable(name)->getInitialValue();
		double sum = 0.0;
		for (AgentNode* agent : agents) {
			const double value = static_cast<RuleWorldModel*>(agent->getWorldModel())->getValue(name, initial);
			collectors.first->getStatistics()->getCollector()->addValue(value);
			sum += value;
		}
		collectors.second->getStatistics()->getCollector()->addValue(sum / agents.size());
	}
}

std::string RuleBehavior::worldModelType() const {
	return Util::TypeOf<RuleWorldModel>();
}

std::vector<std::string> RuleBehavior::producedMessageTypes() const {
	return {Util::TypeOf<RuleMessage>()};
}

std::vector<std::string> RuleBehavior::acceptedMessageTypes() const {
	return {Util::TypeOf<RuleMessage>()};
}

void RuleBehavior::setCondition(const std::string& condition) {
	_condition = condition;
}

std::string RuleBehavior::getCondition() const {
	return _condition;
}

void RuleBehavior::setRules(const std::string& rules) {
	_rules = rules;
}

std::string RuleBehavior::getRules() const {
	return _rules;
}

void RuleBehavior::setMeasuredVariables(const std::string& variables) {
	_measuredVariables = variables;
}

std::string RuleBehavior::getMeasuredVariables() const {
	return _measuredVariables;
}

void RuleBehavior::setPerceptionVariable(const std::string& variable) {
	_perceptionVariable = variable;
}

std::string RuleBehavior::getPerceptionVariable() const {
	return _perceptionVariable;
}

bool RuleBehavior::_loadInstance(PersistenceRecord* fields) {
	bool res = AgentBehavior::_loadInstance(fields);
	if (res) {
		_condition = fields->loadField("condition", std::string(""));
		_rules = fields->loadField("rules", std::string(""));
		_measuredVariables = fields->loadField("measuredVariables", std::string(""));
		_perceptionVariable = fields->loadField("perceptionVariable", std::string(""));
	}
	return res;
}

void RuleBehavior::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	AgentBehavior::_saveInstance(fields, saveDefaultValues);
	fields->saveField("condition", _condition, std::string(""), saveDefaultValues);
	fields->saveField("rules", _rules, std::string(""), saveDefaultValues);
	fields->saveField("measuredVariables", _measuredVariables, std::string(""), saveDefaultValues);
	fields->saveField("perceptionVariable", _perceptionVariable, std::string(""), saveDefaultValues);
}

bool RuleBehavior::_check(std::string& errorMessage) {
	bool resultAll = true;
	auto exists = [&](const std::string& name, const std::string& property) {
		if (_findVariable(name) == nullptr) {
			errorMessage += "RuleBehavior \"" + getName() + "\" " + property + " uses unknown Variable \"" + name + "\". ";
			resultAll = false;
		}
	};
	// The parser replaces the message it receives, so each expression is checked with its own.
	auto check = [&](const std::string& expression, const std::string& property) {
		std::string expressionError;
		if (!_parentModel->checkExpression(expression, "RuleBehavior " + property + "[" + getName() + "]", expressionError)) {
			errorMessage += expressionError;
			resultAll = false;
		}
	};
	for (const auto& rule : parseRules(_rules)) {
		exists(rule.first, "Rules");
		check(rule.second, "Rules");
	}
	if (!_condition.empty()) {
		check(_condition, "Condition");
	}
	for (const std::string& name : splitNames(_measuredVariables)) {
		exists(name, "MeasuredVariables");
	}
	if (!_perceptionVariable.empty()) {
		exists(_perceptionVariable, "PerceptionVariable");
	}
	return resultAll;
}

void RuleBehavior::_createInternalStatisticReporters() {
	_statisticReportersClear();
	_collectors.clear();
	if (_reportStatistics) {
		for (const std::string& name : splitNames(_measuredVariables)) {
			auto* agents = new StatisticsCollector(_parentModel, getName() + "." + name, this);
			auto* average = new StatisticsCollector(_parentModel, getName() + "." + name + ".Average", this);
			_statisticReporterInsert(name, agents);
			_statisticReporterInsert(name + ".Average", average);
			_collectors[name] = {agents, average};
		}
	}
}

void RuleBehavior::_createEditableDataDefinitions() {
	// The Variables are elements declared by the modeler. Attaching them keeps the model check from removing them.
	_optionalEditableDataDefinitionsClear();
	std::set<std::string> names;
	std::vector<std::string> expressions{_condition};
	for (const auto& rule : parseRules(_rules)) {
		names.insert(rule.first);
		expressions.push_back(rule.second);
	}
	for (const std::string& name : splitNames(_measuredVariables)) {
		names.insert(name);
	}
	names.insert(_perceptionVariable);
	for (const std::string& expression : expressions) {
		if (expression.empty()) {
			continue;
		}
		std::map<std::string, std::list<std::string>*> references;
		_parentModel->checkReferencesToDataDefinitions(expression, &references);
		for (auto& [type, referenced] : references) {
			if (type == Util::TypeOf<Variable>()) {
				names.insert(referenced->begin(), referenced->end());
			}
			delete referenced;
		}
	}
	for (const std::string& name : names) {
		if (Variable* variable = _findVariable(name)) {
			_optionalEditableDataDefinitionInsert("Variable_" + name, variable);
		}
	}
}

Variable* RuleBehavior::_findVariable(const std::string& name) const {
	return dynamic_cast<Variable*>(_parentModel->getDataManager()->getDataDefinition(Util::TypeOf<Variable>(), name));
}
