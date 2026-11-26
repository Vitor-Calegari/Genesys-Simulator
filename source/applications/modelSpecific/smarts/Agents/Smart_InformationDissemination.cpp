/*
 * Information dissemination with rule-based agents.
 *
 * A credulous agent becomes informed when it receives a notice, with probability p, and a
 * skeptical agent after it has received two notices. Every informed agent sends a notice to its
 * neighbors at each round, and some agents start informed. The network is a ring, a random
 * network or a small world, all with mean degree 4 and generated with a fixed seed, and each
 * link has a channel in each direction.
 *
 * Arguments, all optional:
 *   --topology=ring|random|smallworld  (ring)
 *   --agents=N                         (20)
 *   --skeptics=FRACTION                (0.25)
 *   --rounds=N                         (20)
 *   --replications=N                   (1)
 *   --trace=0|1                        (1)
 * With the trace on, each round writes one line per agent with its values, from which the
 * fraction of informed agents per round is read.
 */

#include "Smart_InformationDissemination.h"

#include "kernel/simulator/Simulator.h"
#include "plugins/components/Logic/Create.h"
#include "plugins/components/Logic/Dispose.h"
#include "plugins/components/ModalModel/ModalModel.h"
#include "plugins/data/Agents/AgentNetwork.h"
#include "plugins/data/Agents/RuleBehavior.h"
#include "plugins/data/Agents/RuleMessage.h"
#include "plugins/data/Agents/RuleWorldModel.h"
#include "plugins/data/Logic/Variable.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {
// Links between agents, each as a pair (smaller, larger), as in the model of Watts and Strogatz:
// the ring links each agent to the two nearest agents on each side, the small world rewires each
// link of the ring to an agent drawn at random with probability 0.1, and the random network
// rewires every link. Every agent keeps at least the two links it rewires.
std::set<std::pair<int, int>> links(const std::string& topology, int agents, std::mt19937& random) {
	const double rewiring = topology == "smallworld" ? 0.1 : topology == "random" ? 1.0 : 0.0;
	std::set<std::pair<int, int>> result;
	std::uniform_int_distribution<int> anyAgent(0, agents - 1);
	std::uniform_real_distribution<double> unif(0.0, 1.0);
	for (int a = 0; a < agents; a++) {
		for (int step = 1; step <= 2; step++) {
			int b = (a + step) % agents;
			if (unif(random) < rewiring) {
				do {
					b = anyAgent(random);
				} while (b == a || result.count({std::min(a, b), std::max(a, b)}) > 0);
			}
			result.insert({std::min(a, b), std::max(a, b)});
		}
	}
	return result;
}
}

int Smart_InformationDissemination::main(int argc, char** argv) {
	std::map<std::string, std::string> args{{"topology", "ring"}, {"agents", "20"}, {"skeptics", "0.25"}, {"rounds", "20"}, {"replications", "1"}, {"trace", "1"}};
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		arg.erase(0, arg.find_first_not_of('-'));
		const std::size_t equals = arg.find('=');
		args[arg.substr(0, equals)] = equals != std::string::npos ? arg.substr(equals + 1) : "1";
	}
	const std::string topology = args["topology"];
	const int agents = std::stoi(args["agents"]);
	const double skeptics = std::stod(args["skeptics"]);
	const int rounds = std::stoi(args["rounds"]);
	const int replications = std::stoi(args["replications"]);
	const bool trace = args["trace"] != "0";

	Simulator* genesys = new Simulator();
	genesys->getTraceManager()->setTraceLevel(trace ? TraceManager::Level::L5_event : TraceManager::Level::L1_errorFatal);
	setDefaultTraceHandlers(genesys->getTraceManager());
	genesys->getPluginManager()->autoInsertPlugins();
	Model* model = genesys->getModelManager()->newModel();

	// The Variables the rules use. Each agent has its own values for them in its world model.
	new Variable(model, "informado");
	new Variable(model, "recebidos");
	new Variable(model, "ouvidos");
	(new Variable(model, "p"))->setInitialValue(0.5);
	auto behavior = [&](const std::string& name, const std::string& rules) {
		auto* rule = new RuleBehavior(model, name);
		rule->setRules(rules);
		rule->setCondition("informado == 1");
		rule->setPerceptionVariable("recebidos");
		rule->setMeasuredVariables("informado");
		return rule;
	};
	RuleBehavior* credulous = behavior("Credulos", "informado = informado or (recebidos > 0 and UNIF(0, 1) < p)");
	RuleBehavior* skeptical = behavior("Ceticos", "ouvidos = ouvidos + recebidos; informado = informado or ouvidos + recebidos >= 2");

	// The same seed gives the same network and the same skeptics in every execution.
	std::mt19937 random(2026);
	std::vector<int> order(agents);
	std::iota(order.begin(), order.end(), 0);
	std::shuffle(order.begin(), order.end(), random);
	const std::set<int> skeptic(order.begin(), order.begin() + std::lround(skeptics * agents));
	auto* network = new AgentNetwork(model, "Network");
	std::vector<AgentNode*> nodes;
	for (int i = 0; i < agents; i++) {
		char name[16];
		std::snprintf(name, sizeof(name), "Agent%03d", i);
		auto* agent = new AgentNode(model, name);
		agent->setBehavior(skeptic.count(i) > 0 ? skeptical : credulous);
		auto* world = new RuleWorldModel(model);
		if (i % 10 == 0) {
			world->setInitialValue("informado", 1);
		}
		agent->setWorldModel(world);
		network->addNode(agent);
		nodes.push_back(agent);
	}
	for (const auto& [a, b] : links(topology, agents, random)) {
		for (const auto& [source, destination] : {std::pair{a, b}, std::pair{b, a}}) {
			auto* channel = new AgentChannel(model, nodes[source]->getName() + "_" + nodes[destination]->getName());
			channel->setSource(nodes[source]);
			channel->setDestination(nodes[destination]);
			channel->setAcceptedMessageTypes({Util::TypeOf<RuleMessage>()});
			network->addEdge(channel);
		}
	}

	// One round per entity, at the instants 0, 1, 2, ...
	auto* create = new Create(model, "Rounds");
	create->setEntityTypeName("Round");
	create->setFirstCreation(0.0);
	create->setTimeBetweenCreationsExpression("1", Util::TimeUnit::second);
	create->setMaxCreations(rounds);
	auto* modal = new ModalModel(model, "Agents");
	modal->setNetwork(network);
	create->connectTo(modal);
	modal->connectTo(new Dispose(model, "Done"));
	model->getSimulation()->setReplicationLength(rounds, Util::TimeUnit::second);
	model->getSimulation()->setNumberOfReplications(replications);
	model->getSimulation()->setShowReportsAfterReplication(false);
	model->getSimulation()->setShowReportsAfterSimulation(trace);

	if (!model->check()) {
		std::cout << "The model check failed." << std::endl;
		delete genesys;
		return 1;
	}
	std::cout << "topology=" << topology << " agents=" << agents << " skeptics=" << skeptic.size() << " links=" << network->getEdges().size() / 2
			  << " elements=" << model->getDataManager()->getNumberOfDataDefinitions() << " limit=" << genesys->getLicenceManager()->getModelDatasLimit() << std::endl;
	const auto start = std::chrono::steady_clock::now();
	model->getSimulation()->start();
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::cout << "rounds=" << network->getRound() << " replications=" << replications << " seconds=" << seconds
			  << " secondsPerRound=" << seconds / (rounds * replications) << std::endl;
	delete genesys;
	return 0;
}
