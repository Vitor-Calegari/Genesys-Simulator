#include "AgentNetwork.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#include <algorithm>
#include <stdexcept>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &AgentNetwork::GetPluginInformation;
}
#endif

namespace {
bool contains(const std::vector<std::string>& types, const std::string& type) {
	return std::find(types.begin(), types.end(), type) != types.end();
}
}

AgentNetwork::AgentNetwork(Model* model, std::string name)
	: DirectedGraphNetwork(model, name, Util::TypeOf<AgentNetwork>()) {
	addInputPort("tick");
	addOutputPort("completed");
}

AgentNetwork::~AgentNetwork() {
	delete _environment;
}

PluginInformation* AgentNetwork::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<AgentNetwork>(), &AgentNetwork::LoadInstance, &AgentNetwork::NewInstance);
	info->setCategory("Agents");
	info->setDataDefinitionBaseTypeName(Util::TypeOf<DirectedGraphNetwork>());
	info->setDescriptionHelp("Network of agents and channels that runs one synchronous round each time a ModalModel activates it.");
	return info;
}

ModelDataDefinition* AgentNetwork::LoadInstance(Model* model, PersistenceRecord* fields) {
	AgentNetwork* network = new AgentNetwork(model);
	network->_loadInstance(fields);
	return network;
}

ModelDataDefinition* AgentNetwork::NewInstance(Model* model, std::string name) {
	return new AgentNetwork(model, name);
}

bool AgentNetwork::addNode(GraphNode* node) {
	return dynamic_cast<AgentNode*>(node) != nullptr && DirectedGraphNetwork::addNode(node);
}

bool AgentNetwork::addEdge(GraphEdge* edge) {
	return dynamic_cast<AgentChannel*>(edge) != nullptr && DirectedGraphNetwork::addEdge(edge);
}

void AgentNetwork::setEnvironment(AgentEnvironment* environment) {
	if (environment != _environment) {
		delete _environment;
		_environment = environment;
	}
}

AgentEnvironment* AgentNetwork::getEnvironment() const {
	return _environment;
}

unsigned int AgentNetwork::getRound() const {
	return _round;
}

bool AgentNetwork::_check(std::string& errorMessage) {
	bool resultAll = DirectedGraphNetwork::_check(errorMessage);
	for (GraphEdge* edge : getEdges()) {
		auto* channel = static_cast<AgentChannel*>(edge);
		AgentBehavior* source = static_cast<AgentNode*>(channel->getSource())->getBehavior();
		AgentBehavior* destination = static_cast<AgentNode*>(channel->getDestination())->getBehavior();
		bool common = false;
		if (source != nullptr && destination != nullptr) {
			for (const std::string& type : source->producedMessageTypes()) {
				common |= channel->accepts(type) && contains(destination->acceptedMessageTypes(), type);
			}
		}
		if (!common) {
			errorMessage += "AgentChannel \"" + channel->getName() + "\" links agents with no message type in common. ";
			resultAll = false;
		}
	}
	for (AgentNode* agent : _agentsByKey()) {
		AgentBehavior* behavior = agent->getBehavior();
		if (behavior == nullptr || agent->getWorldModel() == nullptr) {
			errorMessage += "Agent \"" + agent->getName() + "\" has no behavior or no world model. ";
			resultAll = false;
			continue;
		}
		if (agent->getWorldModel()->getClassname() != behavior->worldModelType()) {
			errorMessage += "Agent \"" + agent->getName() + "\" has a world model of type " + agent->getWorldModel()->getClassname()
					+ ", but its behavior requires " + behavior->worldModelType() + ". ";
			resultAll = false;
		}
		for (const std::string& type : behavior->producedMessageTypes()) {
			bool accepted = _environment != nullptr && contains(_environment->acceptedMessageTypes(), type);
			for (GraphEdge* edge : getOutgoingEdges(agent)) {
				accepted |= static_cast<AgentChannel*>(edge)->accepts(type);
			}
			if (!accepted) {
				errorMessage += "Agent \"" + agent->getName() + "\" produces message type " + type
						+ ", which neither the environment nor its channels accept. ";
				resultAll = false;
			}
		}
	}
	return resultAll;
}

void AgentNetwork::_initBetweenReplications() {
	DirectedGraphNetwork::_initBetweenReplications();
	_round = 0;
	_broker.clear();
	for (GraphNode* node : getNodes()) {
		static_cast<AgentNode*>(node)->getWorldModel()->reset();
	}
	if (_environment != nullptr) {
		ModelDataDefinition::InitBetweenReplications(_environment);
	}
}

NetworkActivationResult AgentNetwork::_activate(const NetworkActivationFrame& /*frame*/) {
	_round++;
	std::vector<AgentNode*> agents = _agentsByKey();
	std::map<AgentNode*, std::vector<AgentChannel*>> channels = _channelsBySource();
	std::map<AgentNode*, std::unique_ptr<AgentDecision>> decisions;
	std::map<AgentNode*, std::string> failures;

	// Perceive and decide: every agent decides on the state left by the previous round.
	for (AgentNode* agent : agents) {
		AgentPerception perception;
		perception.agent = agent;
		for (AgentChannel* channel : channels[agent]) {
			perception.neighbors.push_back(static_cast<AgentNode*>(channel->getDestination()));
		}
		perception.messages = _broker.inbox(agent);
		if (_environment != nullptr) {
			perception.environment = _environment->perceive(agent);
		}
		try {
			std::unique_ptr<AgentDecision> decision = agent->getBehavior()->decide(perception, agent->getWorldModel());
			for (const std::shared_ptr<AgentMessage>& message : decision->messages) {
				if (!_accepts(agent, channels[agent], message.get())) {
					throw std::invalid_argument("no channel or environment accepts message type \"" + message->getType() + "\"");
				}
			}
			decisions[agent] = std::move(decision);
		} catch (const std::exception& e) {
			failures[agent] = e.what();
		}
	}

	// Apply the valid decisions. An agent whose decision failed receives its messages again.
	std::vector<std::shared_ptr<AgentMessage>> environmentMessages;
	for (AgentNode* agent : agents) {
		auto decision = decisions.find(agent);
		if (decision == decisions.end()) {
			_broker.redeliver(agent);
			continue;
		}
		agent->getBehavior()->apply(decision->second.get(), agent->getWorldModel());
		for (const std::shared_ptr<AgentMessage>& message : decision->second->messages) {
			if (message->getReceiver() != nullptr) {
				_broker.send(message);
			} else {
				environmentMessages.push_back(message);
			}
		}
	}
	_broker.endRound();
	if (_environment != nullptr) {
		_environment->apply(environmentMessages);
	}

	for (AgentNode* agent : agents) {
		auto decision = decisions.find(agent);
		_traceRound(agent, decision != decisions.end() ? decision->second.get() : nullptr, failures[agent]);
	}
	NetworkActivationResult result(getNumOutputPorts());
	result.setPresent(0, _round);
	return result;
}

std::vector<AgentNode*> AgentNetwork::_agentsByKey() const {
	std::vector<AgentNode*> agents;
	for (GraphNode* node : getNodes()) {
		agents.push_back(static_cast<AgentNode*>(node));
	}
	std::sort(agents.begin(), agents.end(), [](AgentNode* a, AgentNode* b) { return a->getName() < b->getName(); });
	return agents;
}

std::map<AgentNode*, std::vector<AgentChannel*>> AgentNetwork::_channelsBySource() const {
	std::map<AgentNode*, std::vector<AgentChannel*>> channels;
	for (GraphEdge* edge : getEdges()) {
		channels[static_cast<AgentNode*>(edge->getSource())].push_back(static_cast<AgentChannel*>(edge));
	}
	return channels;
}

bool AgentNetwork::_accepts(AgentNode* sender, const std::vector<AgentChannel*>& channels, const AgentMessage* message) const {
	if (message->getSender() != sender) {
		return false;
	}
	if (message->getReceiver() == nullptr) {
		return _environment != nullptr && contains(_environment->acceptedMessageTypes(), message->getType());
	}
	for (AgentChannel* channel : channels) {
		if (channel->getDestination() == message->getReceiver() && channel->accepts(message->getType())) {
			return true;
		}
	}
	return false;
}

void AgentNetwork::_traceRound(AgentNode* agent, const AgentDecision* decision, const std::string& failure) {
	std::string line = "AgentNetwork \"" + getName() + "\""
			+ " replication=" + std::to_string(_parentModel->getSimulation()->getCurrentReplicationNumber())
			+ " time=" + Util::StrTruncIfInt(_parentModel->getSimulation()->getSimulatedTime())
			+ " round=" + std::to_string(_round)
			+ " agent=" + agent->getName();
	if (decision == nullptr) {
		line += " failure=\"" + failure + "\"";
	} else {
		line += " decision=\"" + decision->show() + "\" messages=[";
		for (const std::shared_ptr<AgentMessage>& message : decision->messages) {
			line += (message->getReceiver() != nullptr ? message->getReceiver()->getName() : "environment") + ": " + message->show() + "; ";
		}
		line += "]";
	}
	line += " world=\"" + agent->getWorldModel()->show() + "\"";
	traceSimulation(this, TraceManager::Level::L5_event, line);
}
