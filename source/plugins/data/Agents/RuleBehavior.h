#ifndef RULEBEHAVIOR_H
#define RULEBEHAVIOR_H

#include "AgentBehavior.h"
#include "kernel/simulator/PluginInformation.h"

#include <map>
#include <string>
#include <utility>

class StatisticsCollector;
class Variable;

/*!
 * \brief Behavior of rule-based agents, whose state is a set of Variables declared by the modeler.
 *
 * In decide() the Variables take the values of the agent and the number of notices it received,
 * the GenESyS parser evaluates the rules and the condition with the values of the start of the
 * round, as the Set Actions of an EFSM transition, and the Variables get their own values back.
 * When the condition holds the agent sends a RuleMessage to each neighbor.
 */
class RuleBehavior : public AgentBehavior {
public:
	RuleBehavior(Model* model, std::string name = "");
	virtual ~RuleBehavior() override = default;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	virtual std::unique_ptr<AgentDecision> decide(const AgentPerception& perception, const AgentWorldModel* worldModel) override;
	virtual void apply(AgentDecision* decision, AgentWorldModel* worldModel) override;
	virtual void endRound(const std::vector<AgentNode*>& agents) override;
	virtual std::string worldModelType() const override;
	virtual std::vector<std::string> producedMessageTypes() const override;
	virtual std::vector<std::string> acceptedMessageTypes() const override;

public:
	/*! \brief Logical expression, as the guard of an EFSM transition, that makes the agent act. Empty always acts. */
	void setCondition(const std::string& condition);
	std::string getCondition() const;
	/*! \brief Assignments "variable = expression" separated by semicolons, as the Set Actions of an EFSM transition. */
	void setRules(const std::string& rules);
	std::string getRules() const;
	/*! \brief Names of the Variables measured at each round, separated by spaces or commas. */
	void setMeasuredVariables(const std::string& variables);
	std::string getMeasuredVariables() const;
	/*! \brief Variable that receives the number of notices the agent received. */
	void setPerceptionVariable(const std::string& variable);
	std::string getPerceptionVariable() const;

protected:
	virtual bool _loadInstance(PersistenceRecord* fields) override;
	virtual void _saveInstance(PersistenceRecord* fields, bool saveDefaultValues) override;
	virtual bool _check(std::string& errorMessage) override;
	virtual void _createInternalStatisticReporters() override;
	virtual void _createEditableDataDefinitions() override;

private:
	Variable* _findVariable(const std::string& name) const;

private:
	std::string _condition;
	std::string _rules;
	std::string _measuredVariables;
	std::string _perceptionVariable;
	/*! \brief For each measured Variable, the collector of each agent value and the collector of the average over the agents. */
	std::map<std::string, std::pair<StatisticsCollector*, StatisticsCollector*>> _collectors;
};

#endif /* RULEBEHAVIOR_H */
