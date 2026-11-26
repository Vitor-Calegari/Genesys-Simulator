#include "LLMBehavior.h"
#include "AgentNode.h"
#include "LLMDecision.h"
#include "LLMWorldModel.h"
#include "TextMessage.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/essentialPlugins/StatisticsCollector.h"
#include "kernel/simulator/model/Model.h"
#include "plugins/data/AI/AISupport.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <regex>
#include <stdexcept>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &LLMBehavior::GetPluginInformation;
}
#endif

namespace {
// Reads a string field of a JSON response, as AIAssistant does.
bool jsonString(const std::string& json, const std::string& key, std::string& value) {
	std::smatch match;
	std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"])*)\\\"");
	if (!std::regex_search(json, match, pattern)) {
		return false;
	}
	value = match[1].str();
	return true;
}

std::string describe(const AgentPerception& perception) {
	std::string text;
	for (const std::shared_ptr<AgentMessage>& message : perception.environment) {
		text += (message->getSender() != nullptr ? "Public message from " + message->getSender()->getName() : std::string("Environment"))
				+ ": " + message->show() + "\n";
	}
	for (const std::shared_ptr<AgentMessage>& message : perception.messages) {
		text += "Private message from " + message->getSender()->getName() + ": " + message->show() + "\n";
	}
	return text;
}
}

LLMBehavior::LLMBehavior(Model* model, std::string name)
	: AgentBehavior(model, Util::TypeOf<LLMBehavior>(), name) {
	auto* support = new SimulationControlString(
		[this]() { return _aiSupport != nullptr ? _aiSupport->getName() : std::string(""); },
		[this](std::string name) { setAISupport(_findAISupport(_parentModel, name)); },
		Util::TypeOf<LLMBehavior>(), getName(), "AISupport", "AISupport consulted by the agents of this behavior.");
	auto* promptTemplate = new SimulationControlString(
		std::bind(&LLMBehavior::getPromptTemplate, this), std::bind(&LLMBehavior::setPromptTemplate, this, std::placeholders::_1),
		Util::TypeOf<LLMBehavior>(), getName(), "PromptTemplate", "Instructions written at the end of the prompt, after what the agent is, remembers and perceives.");
	promptTemplate->setPreferredEditorHint(SimulationControlEditorHint::MultiLineText);
	for (SimulationControl* control : std::vector<SimulationControl*>{support, promptTemplate}) {
		_parentModel->getControls()->insert(control);
		_addSimulationControl(control);
	}
}

PluginInformation* LLMBehavior::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<LLMBehavior>(), &LLMBehavior::LoadInstance, &LLMBehavior::NewInstance);
	info->setCategory("Agents");
	info->insertDynamicLibFileDependence("aisupport.so");
	info->setDescriptionHelp("Behavior of LLM agents: builds the prompt with the persona, the memory and the perception of each agent and consults an AISupport.");
	return info;
}

ModelDataDefinition* LLMBehavior::LoadInstance(Model* model, PersistenceRecord* fields) {
	// The AISupport is another element of the model. Until it is loaded, the loader tries this behavior again.
	const std::string support = fields->loadField("aiSupport", std::string(""));
	if (!support.empty() && _findAISupport(model, support) == nullptr) {
		return nullptr;
	}
	LLMBehavior* behavior = new LLMBehavior(model);
	behavior->_loadInstance(fields);
	return behavior;
}

ModelDataDefinition* LLMBehavior::NewInstance(Model* model, std::string name) {
	return new LLMBehavior(model, name);
}

std::unique_ptr<AgentDecision> LLMBehavior::decide(const AgentPerception& perception, const AgentWorldModel* worldModel) {
	const auto* world = static_cast<const LLMWorldModel*>(worldModel);
	auto decision = std::make_unique<LLMDecision>();
	decision->perceived = describe(perception);
	std::string receivers;
	for (AgentNode* neighbor : perception.neighbors) {
		receivers += (receivers.empty() ? "" : ", ") + neighbor->getName();
	}
	const std::string prompt = "Agent: " + perception.agent->getName() + "\nPersona: " + world->getPersona()
			+ "\nMemory:\n" + world->getMemory() + "Perceived now:\n" + decision->perceived
			+ "Agents you can send private messages to: " + receivers + "\n" + _promptTemplate;

	// The memory is in the world model, so the conversation in the AISupport is cleared right after the call.
	const std::string key = getName() + ":" + perception.agent->getName();
	const auto start = std::chrono::steady_clock::now();
	const AIConversationResponse answer = _aiSupport->converse(key, prompt);
	decision->latency = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	_aiSupport->clearConversation(key);
	if (!answer.success) {
		throw std::runtime_error(answer.errorMessage);
	}
	decision->tokens = answer.inputTokens + answer.outputTokens;

	std::string action, text, to;
	if (!jsonString(answer.content, "action", action)) {
		throw std::invalid_argument("response without action: " + answer.content);
	}
	jsonString(answer.content, "text", text);
	if (action == "publish") {
		decision->messages.push_back(std::make_shared<TextMessage>(perception.agent, nullptr, text));
		decision->response = "publish: " + text;
	} else if (action == "send") {
		jsonString(answer.content, "to", to);
		auto receiver = std::find_if(perception.neighbors.begin(), perception.neighbors.end(), [&](AgentNode* neighbor) { return neighbor->getName() == to; });
		if (receiver == perception.neighbors.end()) {
			throw std::invalid_argument("response sends to unknown agent \"" + to + "\"");
		}
		decision->messages.push_back(std::make_shared<TextMessage>(perception.agent, *receiver, text));
		decision->response = "send to " + to + ": " + text;
	} else if (action == "none") {
		decision->response = "none";
	} else {
		throw std::invalid_argument("response with unknown action \"" + action + "\"");
	}
	return decision;
}

void LLMBehavior::apply(AgentDecision* decision, AgentWorldModel* worldModel) {
	auto* llmDecision = static_cast<LLMDecision*>(decision);
	static_cast<LLMWorldModel*>(worldModel)->remember("Perceived:\n" + llmDecision->perceived + "Decided: " + llmDecision->response + "\n");
	if (_tokens != nullptr) {
		_tokens->getStatistics()->getCollector()->addValue(llmDecision->tokens);
		_latency->getStatistics()->getCollector()->addValue(llmDecision->latency);
	}
}

std::string LLMBehavior::worldModelType() const {
	return Util::TypeOf<LLMWorldModel>();
}

std::vector<std::string> LLMBehavior::producedMessageTypes() const {
	return {Util::TypeOf<TextMessage>()};
}

std::vector<std::string> LLMBehavior::acceptedMessageTypes() const {
	return {Util::TypeOf<TextMessage>()};
}

void LLMBehavior::setAISupport(AISupport* support) {
	_aiSupport = support;
	if (support != nullptr) {
		_attachedDataInsert("AISupport", support);
	} else {
		_attachedDataRemove("AISupport");
	}
}

AISupport* LLMBehavior::getAISupport() const {
	return _aiSupport;
}

void LLMBehavior::setPromptTemplate(const std::string& promptTemplate) {
	_promptTemplate = promptTemplate;
}

std::string LLMBehavior::getPromptTemplate() const {
	return _promptTemplate;
}

bool LLMBehavior::_loadInstance(PersistenceRecord* fields) {
	bool res = AgentBehavior::_loadInstance(fields);
	if (res) {
		setAISupport(_findAISupport(_parentModel, fields->loadField("aiSupport", std::string(""))));
		_promptTemplate = fields->loadField("promptTemplate", DEFAULT_PROMPT_TEMPLATE);
	}
	return res;
}

void LLMBehavior::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	AgentBehavior::_saveInstance(fields, saveDefaultValues);
	fields->saveField("aiSupport", _aiSupport != nullptr ? _aiSupport->getName() : std::string(""));
	fields->saveField("promptTemplate", _promptTemplate, DEFAULT_PROMPT_TEMPLATE, saveDefaultValues);
}

bool LLMBehavior::_check(std::string& errorMessage) {
	if (_aiSupport == nullptr) {
		errorMessage += "LLMBehavior \"" + getName() + "\" has no AISupport. ";
		return false;
	}
	return true;
}

void LLMBehavior::_createInternalStatisticReporters() {
	_statisticReportersClear();
	_tokens = nullptr;
	_latency = nullptr;
	if (_reportStatistics) {
		_tokens = new StatisticsCollector(_parentModel, getName() + ".Tokens", this);
		_latency = new StatisticsCollector(_parentModel, getName() + ".Latency", this);
		_statisticReporterInsert("Tokens", _tokens);
		_statisticReporterInsert("Latency", _latency);
	}
}

AISupport* LLMBehavior::_findAISupport(Model* model, const std::string& name) {
	return dynamic_cast<AISupport*>(model->getDataManager()->getDataDefinition(Util::TypeOf<AISupport>(), name));
}
