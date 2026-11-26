#ifndef LLMWORLDMODEL_H
#define LLMWORLDMODEL_H

#include "AgentWorldModel.h"
#include "kernel/simulator/PluginInformation.h"

#include <string>

/*!
 * \brief Persona and memory of one LLM agent.
 *
 * The persona is the initial configuration and is saved inside the agent. The memory is the
 * execution state, is not saved and is empty again at each replication.
 */
class LLMWorldModel : public AgentWorldModel {
public:
	LLMWorldModel(Model* model, std::string name = "");
	virtual ~LLMWorldModel() override = default;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	void setPersona(const std::string& persona);
	std::string getPersona() const;
	std::string getMemory() const;
	void remember(const std::string& text);
	virtual void reset() override;
	virtual std::string show() override;

protected:
	virtual bool _loadInstance(PersistenceRecord* fields) override;
	virtual void _saveInstance(PersistenceRecord* fields, bool saveDefaultValues) override;

private:
	std::string _persona;
	std::string _memory;
};

#endif /* LLMWORLDMODEL_H */
