// Unit tests for the agent-based modeling mechanism: one test per item of the verification.

#include <gtest/gtest.h>

#include "kernel/simulator/Simulator.h"
#include "kernel/simulator/essentialPlugins/Counter.h"
#include "kernel/simulator/essentialPlugins/EntityType.h"
#include "kernel/simulator/model/Model.h"
#include "plugins/components/Logic/Create.h"
#include "plugins/components/Logic/Dispose.h"
#include "plugins/components/ModalModel/ModalModel.h"
#include "plugins/data/Agents/AgentNetwork.h"

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

class AgentsTest : public testing::Test {
protected:
	Simulator simulator;
	Model* model = simulator.getModelManager()->newModel();
	AgentNetwork* network = new AgentNetwork(model, "Network");
	// Perceptions of each agent, one per round, in every replication.
	std::map<std::string, std::vector<AgentPerception>> perceived;

	void SetUp() override { simulator.getPluginManager()->autoInsertPlugins(); }

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
	void channel(const std::string& source, const std::string& destination) {
		auto* channel = new AgentChannel(model, source + destination);
		channel->setSource(agent(source));
		channel->setDestination(agent(destination));
		channel->setAcceptedMessageTypes({"TestMessage"});
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
	// Create -> ModalModel -> Dispose, creating \p perCreation entities at the instants 0, 1, 2, ...
	void simulate(unsigned int entities, unsigned int replications = 1, unsigned int perCreation = 1) {
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
		ASSERT_TRUE(model->check());
		model->getSimulation()->start();
	}
};

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
	auto counter = [&](const std::string& name) {
		return static_cast<Counter*>(model->getDataManager()->getDataDefinition(Util::TypeOf<Counter>(), name))->getCountValue();
	};
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
