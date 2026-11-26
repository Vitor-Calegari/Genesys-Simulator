#ifndef RULEWORLDMODEL_H
#define RULEWORLDMODEL_H

#include "AgentWorldModel.h"
#include "kernel/simulator/PluginInformation.h"

#include <map>
#include <string>

/*!
 * \brief Values of one rule-based agent for the Variables its rules use.
 *
 * A Variable without a value here takes its own initial value. The initial values of this world
 * model replace those of the Variables for this agent only.
 */
class RuleWorldModel : public AgentWorldModel {
public:
	RuleWorldModel(Model* model, std::string name = "");
	virtual ~RuleWorldModel() override = default;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	void setInitialValue(const std::string& variable, double value);
	/*! \brief Initial values as "variable = value; ...". */
	void setInitialValues(const std::string& text);
	std::string getInitialValues() const;
	double getValue(const std::string& variable, double defaultValue) const;
	void setValue(const std::string& variable, double value);
	const std::map<std::string, double>& getValues() const;
	virtual void reset() override;
	virtual std::string show() override;

protected:
	virtual bool _loadInstance(PersistenceRecord* fields) override;
	virtual void _saveInstance(PersistenceRecord* fields, bool saveDefaultValues) override;

private:
	std::map<std::string, double> _initialValues;
	std::map<std::string, double> _values;
};

#endif /* RULEWORLDMODEL_H */
