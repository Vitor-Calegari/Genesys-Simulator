// Unit tests for the agent-based modeling mechanism: one test per item of the verification.

#include <gtest/gtest.h>

#include "kernel/simulator/Simulator.h"
#include "kernel/simulator/essentialPlugins/Counter.h"
#include "kernel/simulator/essentialPlugins/EntityType.h"
#include "kernel/simulator/model/Model.h"
#include "plugins/components/Logic/Create.h"
#include "plugins/components/Logic/Dispose.h"
#include "plugins/components/ModalModel/ModalModel.h"
#include "plugins/data/AI/AISupport.h"
#include "plugins/data/Agents/AgentNetwork.h"
#include "plugins/data/Agents/RuleBehavior.h"
#include "plugins/data/Agents/RuleWorldModel.h"
#include "plugins/data/Logic/Variable.h"
#include "kernel/simulator/essentialPlugins/StatisticsCollector.h"

#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TestMessage : public AgentMessage {
public:
	TestMessage(AgentNode* sender, AgentNode* receiver, std::string text, std::string type)
		: AgentMessage(type, sender, receiver), text(std::move(text)) {}
	std::string show() const override { return text; }
	std::string text;
};

class TestWorldModel : public AgentWorldModel {
public:
	explicit TestWorldModel(Model* model, std::string type = "TestWorldModel") : AgentWorldModel(model, type) {}
	void reset() override { value = initial; }
	std::string show() override { return std::to_string(value); }
	int initial = 0;
	int value = 0;
};

class TestDecision : public AgentDecision {
public:
	int value = 0;
};

// decide() is given by each test, and apply() stores the decided value in the world model.
class TestBehavior : public AgentBehavior {
public:
	using Decide = std::function<void(const AgentPerception&, const TestWorldModel*, TestDecision*)>;
	TestBehavior(Model* model, Decide decide) : AgentBehavior(model, "TestBehavior"), _decide(std::move(decide)) {}
	std::unique_ptr<AgentDecision> decide(const AgentPerception& perception, const AgentWorldModel* worldModel) override {
		auto decision = std::make_unique<TestDecision>();
		decision->value = static_cast<const TestWorldModel*>(worldModel)->value;
		_decide(perception, static_cast<const TestWorldModel*>(worldModel), decision.get());
		return decision;
	}
	void apply(AgentDecision* decision, AgentWorldModel* worldModel) override {
		static_cast<TestWorldModel*>(worldModel)->value = static_cast<TestDecision*>(decision)->value;
	}
	std::string worldModelType() const override { return "TestWorldModel"; }
	std::vector<std::string> producedMessageTypes() const override { return {"TestMessage"}; }
	std::vector<std::string> acceptedMessageTypes() const override { return {"TestMessage"}; }

private:
	Decide _decide;
};

// Keeps every message it receives, in arrival order, and shows all of them to every agent.
class TestEnvironment : public AgentEnvironment {
public:
	explicit TestEnvironment(Model* model) : AgentEnvironment(model, "TestEnvironment") {}
	std::vector<std::shared_ptr<AgentMessage>> perceive(AgentNode*) const override { return received; }
	void apply(const std::vector<std::shared_ptr<AgentMessage>>& messages) override {
		received.insert(received.end(), messages.begin(), messages.end());
	}
	std::vector<std::string> acceptedMessageTypes() const override { return {"TestMessage"}; }
	std::vector<std::shared_ptr<AgentMessage>> received;

protected:
	void _initBetweenReplications() override { received.clear(); }
};

std::vector<std::string> texts(const std::vector<std::shared_ptr<AgentMessage>>& messages) {
	std::vector<std::string> result;
	for (const auto& message : messages) result.push_back(message->show());
	return result;
}

// A model with one AgentNetwork, activated by Create -> ModalModel -> Dispose.
class AgentModel {
public:
	Simulator simulator;
	Model* model = simulator.getModelManager()->newModel();
	AgentNetwork* network = new AgentNetwork(model, "Network");
	// Perceptions of each agent, one per round, in every replication.
	std::map<std::string, std::vector<AgentPerception>> perceived;

	AgentModel() { simulator.getPluginManager()->autoInsertPlugins(); }

	// Creates the agents in the given order, all with one behavior whose decide() is \p decide.
	void agents(const std::vector<std::string>& names, TestBehavior::Decide decide) {
		auto* behavior = new TestBehavior(model, [this, decide](const AgentPerception& perception, const TestWorldModel* world, TestDecision* decision) {
			perceived[perception.agent->getName()].push_back(perception);
			decide(perception, world, decision);
		});
		for (const std::string& name : names) {
			auto* agent = new AgentNode(model, name);
			agent->setBehavior(behavior);
			agent->setWorldModel(new TestWorldModel(model));
			network->addNode(agent);
		}
	}
	AgentNode* agent(const std::string& name) { return static_cast<AgentNode*>(network->findNodeByName(name)); }
	TestWorldModel* world(const std::string& name) { return static_cast<TestWorldModel*>(agent(name)->getWorldModel()); }
	// Creates rule-based agents that share one RuleBehavior, each with the given initial values.
	RuleBehavior* ruleAgents(const std::map<std::string, std::string>& initialValues) {
		auto* behavior = new RuleBehavior(model, "Rules");
		for (const auto& [name, values] : initialValues) {
			auto* agent = new AgentNode(model, name);
			agent->setBehavior(behavior);
			auto* world = new RuleWorldModel(model);
			world->setInitialValues(values);
			agent->setWorldModel(world);
			network->addNode(agent);
		}
		return behavior;
	}
	RuleWorldModel* ruleWorld(const std::string& name) { return static_cast<RuleWorldModel*>(agent(name)->getWorldModel()); }
	void channel(const std::string& source, const std::string& destination, const std::string& type = "TestMessage") {
		auto* channel = new AgentChannel(model, source + destination);
		channel->setSource(agent(source));
		channel->setDestination(agent(destination));
		channel->setAcceptedMessageTypes({type});
		network->addEdge(channel);
	}
	TestEnvironment* environment() {
		auto* environment = new TestEnvironment(model);
		network->setEnvironment(environment);
		return environment;
	}
	std::shared_ptr<AgentMessage> message(AgentNode* sender, AgentNode* receiver, std::string text = "", std::string type = "TestMessage") {
		return std::make_shared<TestMessage>(sender, receiver, text, type);
	}
	double now() { return model->getSimulation()->getSimulatedTime(); }
	double counter(const std::string& name) {
		return static_cast<Counter*>(model->getDataManager()->getDataDefinition(Util::TypeOf<Counter>(), name))->getCountValue();
	}
	// Create -> ModalModel -> Dispose, creating \p perCreation entities at the instants 0, 1, 2, ...
	void simulate(unsigned int entities, unsigned int replications = 1, unsigned int perCreation = 1) {
		flow(entities, replications, perCreation);
		ASSERT_TRUE(model->check());
		model->getSimulation()->start();
	}
	void flow(unsigned int entities, unsigned int replications = 1, unsigned int perCreation = 1) {
		auto* create = new Create(model, "Create");
		create->setEntityType(new EntityType(model, "Token"));
		create->setFirstCreation(0.0);
		create->setTimeBetweenCreationsExpression("1");
		create->setEntitiesPerCreation(perCreation);
		create->setMaxCreations(entities);
		auto* modal = new ModalModel(model, "Modal");
		modal->setNetwork(network);
		create->connectTo(modal);
		modal->connectTo(new Dispose(model, "Dispose"));
		model->getSimulation()->setReplicationLength(100.0, Util::TimeUnit::second);
		model->getSimulation()->setNumberOfReplications(replications);
	}
};

class AgentsTest : public testing::Test, public AgentModel {};

// Credulous agents in a ring where only A starts informed, as in the dissemination example.
std::map<std::string, double> disseminate() {
	AgentModel ring;
	new Variable(ring.model, "informado");
	new Variable(ring.model, "recebidos");
	(new Variable(ring.model, "p"))->setInitialValue(0.5);
	RuleBehavior* credulous = ring.ruleAgents({{"A", "informado=1"}, {"B", ""}, {"C", ""}, {"D", ""}, {"E", ""}, {"F", ""}});
	credulous->setRules("informado = informado or (recebidos > 0 and UNIF(0, 1) < p)");
	credulous->setCondition("informado == 1");
	credulous->setPerceptionVariable("recebidos");
	const std::vector<std::string> names{"A", "B", "C", "D", "E", "F"};
	for (std::size_t i = 0; i < names.size(); i++) {
		ring.channel(names[i], names[(i + 1) % names.size()], "RuleMessage");
	}
	ring.simulate(8);
	std::map<std::string, double> informed;
	for (const std::string& name : names) {
		informed[name] = ring.ruleWorld(name)->getValue("informado", 0.0);
	}
	return informed;
}

} // namespace

TEST_F(AgentsTest, EachEntityRunsExactlyOneRound) {
	std::vector<double> times;
	agents({"A", "B"}, [&](const AgentPerception& perception, const TestWorldModel*, TestDecision* decision) {
		if (perception.agent->getName() == "A") {
			times.push_back(now());
			decision->messages.push_back(message(perception.agent, agent("B")));
		}
	});
	channel("A", "B");
	channel("B", "A");
	simulate(4, 1, 2);
	EXPECT_EQ(times, (std::vector<double>{0, 0, 1, 1}));
	EXPECT_EQ(network->getRound(), 4u);
	// The second round at the same instant perceives the result of the first one.
	EXPECT_TRUE(perceived["B"][0].messages.empty());
	EXPECT_EQ(perceived["B"][1].messages.size(), 1u);
}

TEST_F(AgentsTest, MessagesArePerceivedOnlyInTheNextRound) {
	agents({"A", "B"}, [&](const AgentPerception& perception, const TestWorldModel*, TestDecision* decision) {
		if (perception.agent->getName() == "A" && network->getRound() == 1) {
			decision->messages.push_back(message(perception.agent, agent("B"), "private"));
			decision->messages.push_back(message(perception.agent, nullptr, "public"));
		}
	});
	channel("A", "B");
	channel("B", "A");
	environment();
	simulate(2);
	EXPECT_TRUE(perceived["B"][0].messages.empty());
	EXPECT_TRUE(perceived["B"][0].environment.empty());
	EXPECT_EQ(texts(perceived["B"][1].messages), (std::vector<std::string>{"private"}));
	EXPECT_EQ(texts(perceived["B"][1].environment), (std::vector<std::string>{"public"}));
}

TEST_F(AgentsTest, PerceptionDoesNotDependOnAgentOrder) {
	agents({"B", "A"}, [&](const AgentPerception& perception, const TestWorldModel*, TestDecision* decision) {
		if (network->getRound() == 1) {
			decision->messages.push_back(message(perception.agent, perception.neighbors[0], perception.agent->getName()));
			decision->messages.push_back(message(perception.agent, nullptr, perception.agent->getName()));
		}
	});
	channel("A", "B");
	channel("B", "A");
	environment();
	simulate(2);
	// A decides before B, and still B does not perceive what A sent in the same round.
	for (const std::string name : {"A", "B"}) {
		EXPECT_TRUE(perceived[name][0].messages.empty());
		EXPECT_TRUE(perceived[name][0].environment.empty());
		EXPECT_EQ(perceived[name][1].messages.size(), 1u);
		EXPECT_EQ(texts(perceived[name][1].environment), (std::vector<std::string>{"A", "B"}));
	}
}

TEST_F(AgentsTest, TwoReplicationsStartFromTheSameState) {
	std::vector<int> values;
	agents({"A", "B"}, [&](const AgentPerception& perception, const TestWorldModel* world, TestDecision* decision) {
		decision->value = world->value + 1;
		if (perception.agent->getName() == "A") {
			values.push_back(world->value);
			decision->messages.push_back(message(perception.agent, agent("B")));
			decision->messages.push_back(message(perception.agent, nullptr));
		}
	});
	channel("A", "B");
	channel("B", "A");
	environment();
	world("A")->initial = 5;
	simulate(2, 2);
	EXPECT_EQ(values, (std::vector<int>{5, 6, 5, 6}));
	ASSERT_EQ(perceived["B"].size(), 4u);
	EXPECT_TRUE(perceived["B"][2].messages.empty());
	EXPECT_TRUE(perceived["B"][2].environment.empty());
	EXPECT_EQ(perceived["B"][3].messages.size(), perceived["B"][1].messages.size());
	EXPECT_EQ(perceived["B"][3].environment.size(), perceived["B"][1].environment.size());
}

TEST_F(AgentsTest, FailureKeepsWorldModelAndMessagesAndSimulationContinues) {
	agents({"A", "B"}, [&](const AgentPerception& perception, const TestWorldModel* world, TestDecision* decision) {
		decision->value = world->value + 1;
		if (perception.agent->getName() == "A" && network->getRound() == 1) {
			decision->messages.push_back(message(perception.agent, agent("B"), "hello"));
		}
		if (perception.agent->getName() == "B" && network->getRound() == 2) {
			throw std::runtime_error("provider unavailable");
		}
	});
	channel("A", "B");
	channel("B", "A");
	simulate(3);
	EXPECT_EQ(network->getRound(), 3u);
	EXPECT_EQ(network->getReplicationFailures(), 1u);
	EXPECT_EQ(world("A")->value, 3);
	EXPECT_EQ(world("B")->value, 2);
	EXPECT_EQ(texts(perceived["B"][1].messages), (std::vector<std::string>{"hello"}));
	EXPECT_EQ(texts(perceived["B"][2].messages), (std::vector<std::string>{"hello"}));
}

TEST_F(AgentsTest, FailureLimitCancelsTheRoundAndStopsTheSimulation) {
	// B fails in the second round of each replication, and the limit counts failures over all replications.
	agents({"A", "B"}, [&](const AgentPerception& perception, const TestWorldModel* world, TestDecision* decision) {
		decision->value = world->value + 1;
		if (perception.agent->getName() == "B" && network->getRound() == 2) {
			throw std::runtime_error("invalid response");
		}
	});
	channel("A", "B");
	channel("B", "A");
	network->setFailureLimit(2);
	network->setFailureLimitPerReplication(false);
	simulate(3, 2);
	EXPECT_EQ(perceived["A"].size(), 5u);
	EXPECT_EQ(world("A")->value, 1);
	EXPECT_EQ(network->getTotalFailures(), 2u);
	EXPECT_EQ(counter("Network.Failures"), 1.0);
	EXPECT_EQ(counter("Network.TotalFailures"), 2.0);
}

TEST_F(AgentsTest, MessageWithoutAcceptingChannelOrEnvironmentFailsTheDecision) {
	agents({"A", "B", "C"}, [&](const AgentPerception& perception, const TestWorldModel*, TestDecision* decision) {
		decision->value = 1;
		const std::string name = perception.agent->getName();
		if (name == "A") decision->messages.push_back(message(perception.agent, agent("C")));  // no channel from A to C
		if (name == "B") decision->messages.push_back(message(perception.agent, agent("A"), "", "Other"));  // channel does not accept the type
		if (name == "C") decision->messages.push_back(message(perception.agent, nullptr, "", "Other"));  // environment does not accept the type
	});
	channel("A", "B");
	channel("B", "A");
	channel("C", "A");
	environment();
	simulate(1);
	EXPECT_EQ(world("A")->value, 0);
	EXPECT_EQ(world("B")->value, 0);
	EXPECT_EQ(world("C")->value, 0);
}

TEST_F(AgentsTest, EnvironmentReceivesMessagesAlwaysInTheSameOrder) {
	agents({"B", "A"}, [&](const AgentPerception& perception, const TestWorldModel*, TestDecision* decision) {
		decision->messages.push_back(message(perception.agent, nullptr, perception.agent->getName()));
	});
	TestEnvironment* shared = environment();
	simulate(1);
	EXPECT_EQ(texts(shared->received), (std::vector<std::string>{"A", "B"}));
}

TEST_F(AgentsTest, ModelCheckRejectsTheThreeCases) {
	agents({"A", "B"}, [](const AgentPerception&, const TestWorldModel*, TestDecision*) {});
	channel("A", "B");
	channel("B", "A");
	std::string error;
	EXPECT_TRUE(ModelDataDefinition::Check(network, error)) << error;

	auto* ab = static_cast<AgentChannel*>(network->findEdgeByName("AB"));
	ab->setAcceptedMessageTypes({"Other"});
	error.clear();
	EXPECT_FALSE(ModelDataDefinition::Check(network, error));
	EXPECT_NE(error.find("AgentChannel \"AB\" links agents with no message type in common"), std::string::npos);
	ab->setAcceptedMessageTypes({"TestMessage"});

	network->removeEdge(network->findEdgeByName("BA"));
	error.clear();
	EXPECT_FALSE(ModelDataDefinition::Check(network, error));
	EXPECT_NE(error.find("Agent \"B\" produces message type TestMessage"), std::string::npos);
	channel("B", "A");

	agent("A")->setWorldModel(new TestWorldModel(model, "OtherWorldModel"));
	error.clear();
	EXPECT_FALSE(ModelDataDefinition::Check(network, error));
	EXPECT_NE(error.find("Agent \"A\" has a world model of type OtherWorldModel"), std::string::npos);
}

TEST_F(AgentsTest, SaveKeepsEachWorldModelInsideItsAgent) {
	agents({"A", "B"}, [](const AgentPerception&, const TestWorldModel*, TestDecision*) {});
	channel("A", "B");
	channel("B", "A");
	environment();
	network->setFailureLimit(3);
	const std::string filename = testing::TempDir() + "genesys_test_agents.gen";
	ASSERT_TRUE(model->save(filename));
	std::ifstream file(filename);
	const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	std::remove(filename.c_str());
	EXPECT_NE(text.find("AgentNode  \"A\" worldModel.typename=\"TestWorldModel\""), std::string::npos);
	EXPECT_NE(text.find("graphNode[0].worldModel.typename=\"TestWorldModel\""), std::string::npos);
	EXPECT_NE(text.find("environment.typename=\"TestEnvironment\""), std::string::npos);
	EXPECT_NE(text.find("graphEdge[0].acceptedMessageTypes=\"TestMessage\""), std::string::npos);
	EXPECT_NE(text.find("failureLimit=3"), std::string::npos);
	EXPECT_EQ(text.find("TestWorldModel \""), std::string::npos);
	EXPECT_EQ(text.find("TestEnvironment \""), std::string::npos);
}

TEST_F(AgentsTest, PropertiesAreEditableInThePropertyEditor) {
	agents({"A", "B"}, [](const AgentPerception&, const TestWorldModel*, TestDecision*) {});
	channel("A", "B");
	environment();
	auto control = [](ModelDataDefinition* element, const std::string& name) -> SimulationControl* {
		for (SimulationControl* control : *element->getSimulationControls()->list()) {
			if (control->getName() == name) return control;
		}
		return nullptr;
	};
	auto* other = new TestBehavior(model, [](const AgentPerception&, const TestWorldModel*, TestDecision*) {});
	control(agent("A"), "Behavior")->setValue(other->getName());
	EXPECT_EQ(agent("A")->getBehavior(), other);
	EXPECT_EQ(control(agent("A"), "Behavior")->getValue(), other->getName());
	SimulationControl* worldModel = control(agent("A"), "WorldModel");
	EXPECT_TRUE(worldModel->isInlineObjectProperty());
	EXPECT_TRUE(worldModel->isReadOnly());
	EXPECT_EQ(worldModel->getChildSimulationControls(), world("A")->getSimulationControls());
	EXPECT_EQ(control(network, "Environment")->getChildSimulationControls(), network->getEnvironment()->getSimulationControls());

	auto* ab = static_cast<AgentChannel*>(network->findEdgeByName("AB"));
	control(ab, "AcceptedMessageTypes")->setValue("TestMessage, OtherMessage");
	EXPECT_EQ(ab->acceptedMessageTypes(), (std::vector<std::string>{"TestMessage", "OtherMessage"}));
	EXPECT_EQ(control(ab, "AcceptedMessageTypes")->getValue(), "TestMessage OtherMessage");

	control(network, "FailureLimit")->setValue("2");
	control(network, "FailureLimitPerReplication")->setValue("false");
	EXPECT_EQ(network->getFailureLimit(), 2u);
	EXPECT_FALSE(network->isFailureLimitPerReplication());
}

TEST_F(AgentsTest, CountersCountDecisionsAndMessagesOfEachRound) {
	agents({"A", "B"}, [&](const AgentPerception& perception, const TestWorldModel*, TestDecision* decision) {
		if (perception.agent->getName() == "A") {
			decision->messages.push_back(message(perception.agent, agent("B")));
			decision->messages.push_back(message(perception.agent, nullptr));
		}
		if (perception.agent->getName() == "B" && network->getRound() == 2) {
			throw std::runtime_error("invalid response");
		}
	});
	channel("A", "B");
	channel("B", "A");
	environment();
	simulate(3);
	EXPECT_EQ(counter("Network.Decisions"), 5.0);
	EXPECT_EQ(counter("Network.Messages"), 6.0);
	EXPECT_EQ(counter("Network.Failures"), 1.0);
}

TEST_F(AgentsTest, SaveAndLoadKeepAgentsChannelsAndWorldModels) {
	new Variable(model, "x");
	ruleAgents({{"A", "x=10"}, {"B", "x=20"}})->setRules("x = x + 1");
	channel("A", "B", "RuleMessage");
	channel("B", "A", "RuleMessage");
	network->setFailureLimit(3);
	flow(2);
	const std::string filename = testing::TempDir() + "genesys_test_agents_rules.gen";
	ASSERT_TRUE(model->save(filename));

	Model* loaded = simulator.getModelManager()->newModel();
	ASSERT_TRUE(loaded->load(filename));
	std::remove(filename.c_str());
	auto* network = dynamic_cast<AgentNetwork*>(loaded->getDataManager()->getDataDefinition(Util::TypeOf<AgentNetwork>(), "Network"));
	ASSERT_NE(network, nullptr);
	EXPECT_EQ(network->getFailureLimit(), 3u);
	EXPECT_EQ(loaded->getDataManager()->getNumberOfDataDefinitions(Util::TypeOf<AgentNode>()), 2u);
	EXPECT_EQ(loaded->getDataManager()->getNumberOfDataDefinitions(Util::TypeOf<RuleWorldModel>()), 0u);
	auto* a = static_cast<AgentNode*>(network->findNodeByName("A"));
	auto* b = static_cast<AgentNode*>(network->findNodeByName("B"));
	EXPECT_EQ(static_cast<RuleBehavior*>(a->getBehavior())->getRules(), "x = x + 1");
	EXPECT_EQ(b->getBehavior(), a->getBehavior());
	EXPECT_EQ(dynamic_cast<RuleWorldModel*>(a->getWorldModel())->getInitialValues(), "x=10");
	EXPECT_EQ(dynamic_cast<RuleWorldModel*>(b->getWorldModel())->getInitialValues(), "x=20");
	auto* ab = static_cast<AgentChannel*>(network->findEdgeByName("AB"));
	EXPECT_EQ(ab->getSource(), a);
	EXPECT_EQ(ab->getDestination(), b);
	EXPECT_EQ(ab->acceptedMessageTypes(), (std::vector<std::string>{"RuleMessage"}));
	EXPECT_EQ(network->getEdges().size(), 2u);

	ASSERT_TRUE(loaded->check());
	loaded->getSimulation()->start();
	EXPECT_EQ(network->getRound(), 2u);
	EXPECT_EQ(dynamic_cast<RuleWorldModel*>(a->getWorldModel())->getValue("x", 0), 12.0);
}

TEST_F(AgentsTest, RulesUseOnlyTheAgentValuesAndVariablesKeepTheirOwn) {
	auto* x = new Variable(model, "x");
	x->setInitialValue(100);
	RuleBehavior* behavior = ruleAgents({{"A", "x=10"}, {"B", "x=20"}});
	behavior->setRules("x = x + 1");
	behavior->setCondition("0");
	behavior->setMeasuredVariables("x");
	channel("A", "B", "RuleMessage");
	channel("B", "A", "RuleMessage");
	simulate(3);
	EXPECT_EQ(ruleWorld("A")->getValue("x", 0), 13.0);
	EXPECT_EQ(ruleWorld("B")->getValue("x", 0), 23.0);
	EXPECT_EQ(x->getValue(), 100.0);
	auto collector = [&](const std::string& name) {
		return static_cast<StatisticsCollector*>(model->getDataManager()->getDataDefinition(Util::TypeOf<StatisticsCollector>(), name))->getStatistics();
	};
	// One value of each agent per round, and one average over the agents per round.
	EXPECT_EQ(collector("Rules.x")->numElements(), 6u);
	EXPECT_EQ(collector("Rules.x.Average")->numElements(), 3u);
	EXPECT_DOUBLE_EQ(collector("Rules.x.Average")->average(), 17.0);
}

TEST(AgentsDisseminationTest, SameSeedGivesTheSameResult) {
	const std::map<std::string, double> first = disseminate();
	EXPECT_EQ(first, disseminate());
	EXPECT_EQ(first.at("A"), 1.0);
	EXPECT_GT(first.at("B"), 0.0);
}

TEST_F(AgentsTest, RuleCheckRejectsUnknownVariables) {
	new Variable(model, "x");
	auto* behavior = new RuleBehavior(model, "Rules");
	behavior->setRules("x = x + 1; y = x");
	behavior->setCondition("w > 0");
	behavior->setMeasuredVariables("x, z");
	std::string error;
	EXPECT_FALSE(ModelDataDefinition::Check(behavior, error));
	EXPECT_NE(error.find("Rules uses unknown Variable \"y\""), std::string::npos) << error;
	EXPECT_NE(error.find("MeasuredVariables uses unknown Variable \"z\""), std::string::npos);
	EXPECT_NE(error.find("Expression \"w > 0\""), std::string::npos);
}

TEST_F(AgentsTest, SimulatedProviderGivesEachAgentItsNthResponseAndRestartsEachReplication) {
	auto* support = new AISupport(model, "LLM");
	support->setProvider("Simulated");
	support->setSimulatedResponses("A: a1\nB: b1\nA: a2\n");
	auto ask = [&](const std::string& agent) { return support->converse(agent, "Agent: " + agent + "\nPersona: none").content; };
	EXPECT_EQ(ask("B"), "b1");
	EXPECT_EQ(ask("A"), "a1");
	EXPECT_EQ(ask("A"), "a2");
	EXPECT_EQ(ask("B"), "{\"action\": \"none\"}");
	ModelDataDefinition::InitBetweenReplications(support);
	EXPECT_EQ(ask("A"), "a1");
}
