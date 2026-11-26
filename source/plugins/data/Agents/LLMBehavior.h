#ifndef LLMBEHAVIOR_H
#define LLMBEHAVIOR_H

#include "AgentBehavior.h"
#include "kernel/simulator/PluginInformation.h"

#include <string>

class AISupport;
class StatisticsCollector;

/*!
 * \brief Behavior of LLM agents, which decide by consulting an LLM through an AISupport.
 *
 * The prompt starts with the line "Agent: name" and has the persona and the memory of the
 * agent, what it perceived and the agents it can send private messages to, followed by the
 * prompt template. The response is a JSON object whose "action" is "publish", with a "text"
 * sent to the environment, "send", with a "text" sent to the agent named in "to", or "none".
 * apply() keeps in the memory what the agent perceived and decided, and records the tokens
 * and the latency of the call.
 */
class LLMBehavior : public AgentBehavior {
public:
	LLMBehavior(Model* model, std::string name = "");
	virtual ~LLMBehavior() override = default;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	virtual std::unique_ptr<AgentDecision> decide(const AgentPerception& perception, const AgentWorldModel* worldModel) override;
	virtual void apply(AgentDecision* decision, AgentWorldModel* worldModel) override;
	virtual std::string worldModelType() const override;
	virtual std::vector<std::string> producedMessageTypes() const override;
	virtual std::vector<std::string> acceptedMessageTypes() const override;

public:
	void setAISupport(AISupport* support);
	AISupport* getAISupport() const;
	/*! \brief Instructions written at the end of the prompt, after what the agent is, remembers and perceives. */
	void setPromptTemplate(const std::string& promptTemplate);
	std::string getPromptTemplate() const;

protected:
	virtual bool _loadInstance(PersistenceRecord* fields) override;
	virtual void _saveInstance(PersistenceRecord* fields, bool saveDefaultValues) override;
	virtual bool _check(std::string& errorMessage) override;
	virtual void _createInternalStatisticReporters() override;

private:
	static AISupport* _findAISupport(Model* model, const std::string& name);

private:
	static inline const std::string DEFAULT_PROMPT_TEMPLATE =
			"Answer with one JSON object and nothing else: {\"action\": \"publish\", \"text\": \"...\"} to publish a statement, "
			"{\"action\": \"send\", \"to\": \"agent\", \"text\": \"...\"} to send a private message to one of the agents above, "
			"or {\"action\": \"none\"} to do nothing.";
	AISupport* _aiSupport = nullptr;
	std::string _promptTemplate = DEFAULT_PROMPT_TEMPLATE;
	StatisticsCollector* _tokens = nullptr;
	StatisticsCollector* _latency = nullptr;
};

#endif /* LLMBEHAVIOR_H */
