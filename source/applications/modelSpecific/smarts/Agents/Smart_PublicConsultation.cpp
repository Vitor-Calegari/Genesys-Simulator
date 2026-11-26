/*
 * Public consultation on a bill that reduces the working week, with three LLM agents.
 *
 * The ministry, the union and the business association are LLM agents, each with its persona,
 * linked by channels in both directions between the ministry and each of the others. The
 * environment is the consultation, with the text of the law and the published statements. At
 * each round each institution may publish a statement, send a private message through one of
 * its channels or do nothing.
 *
 * Arguments, all optional:
 *   --mode=controlled|demo  (controlled)
 *   --rounds=N              (4)
 * In the controlled mode the simulated provider of the AISupport answers with the responses of
 * the controlled mode of the report. In the demonstration mode the AISupport uses the LLM
 * provider set by the environment variables GENESYS_AI_PROVIDER, GENESYS_AI_MODEL,
 * GENESYS_AI_BASE_URL and GENESYS_AI_API_KEY_ENV, as in StartAIAssistant. The key itself stays
 * in the variable named by GENESYS_AI_API_KEY_ENV and is never written to the model.
 */

#include "Smart_PublicConsultation.h"

#include "kernel/simulator/Simulator.h"
#include "plugins/components/Logic/Create.h"
#include "plugins/components/Logic/Dispose.h"
#include "plugins/components/ModalModel/ModalModel.h"
#include "plugins/data/AI/AISupport.h"
#include "plugins/data/Agents/AgentNetwork.h"
#include "plugins/data/Agents/LLMBehavior.h"
#include "plugins/data/Agents/LLMWorldModel.h"
#include "plugins/data/Agents/PublicConsultationEnvironment.h"
#include "plugins/data/Agents/TextMessage.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace {
std::string envOr(const char* name, const std::string& fallback) {
	const char* value = std::getenv(name);
	return (value != nullptr && *value != '\0') ? std::string(value) : fallback;
}

// Decisions of the controlled mode, one per institution and round, as in the table of the report.
const std::string CONTROLLED_MODE =
	"Ministerio: {\"action\": \"publish\", \"text\": \"A redução da jornada protege a saúde dos trabalhadores e mantém os salários.\"}\n"
	"Ministerio: {\"action\": \"none\"}\n"
	"Ministerio: {\"action\": \"send\", \"to\": \"Associacao\", \"text\": \"Propomos compensação fiscal às pequenas empresas durante a transição.\"}\n"
	"Ministerio: {\"action\": \"none\"}\n"
	"Sindicato: {\"action\": \"none\"}\n"
	"Sindicato: {\"action\": \"publish\", \"text\": \"Apoiamos a lei, desde que não haja redução de salário.\"}\n"
	"Sindicato: Concordo com tudo.\n"
	"Sindicato: {\"action\": \"none\"}\n"
	"Associacao: {\"action\": \"none\"}\n"
	"Associacao: {\"action\": \"send\", \"to\": \"Ministerio\", \"text\": \"Pedimos compensação para as pequenas empresas.\"}\n"
	"Associacao: {\"action\": \"none\"}\n"
	"Associacao: {\"action\": \"publish\", \"text\": \"Apoiamos a lei com a compensação proposta pelo ministério.\"}\n";
}

int Smart_PublicConsultation::main(int argc, char** argv) {
	std::map<std::string, std::string> args{{"mode", "controlled"}, {"rounds", "4"}};
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		arg.erase(0, arg.find_first_not_of('-'));
		const std::size_t equals = arg.find('=');
		args[arg.substr(0, equals)] = equals != std::string::npos ? arg.substr(equals + 1) : "1";
	}
	const bool demo = args["mode"] == "demo";
	const int rounds = std::stoi(args["rounds"]);

	Simulator* genesys = new Simulator();
	genesys->getTraceManager()->setTraceLevel(TraceManager::Level::L5_event);
	setDefaultTraceHandlers(genesys->getTraceManager());
	genesys->getPluginManager()->autoInsertPlugins();
	Model* model = genesys->getModelManager()->newModel();

	auto* support = new AISupport(model, "LLM");
	if (demo) {
		support->setProvider(envOr("GENESYS_AI_PROVIDER", "Local"));
		support->setModel(envOr("GENESYS_AI_MODEL", "llama3.2"));
		support->setBaseUrl(envOr("GENESYS_AI_BASE_URL", ""));
		support->setApiKeyEnvironmentVariable(envOr("GENESYS_AI_API_KEY_ENV", ""));
	} else {
		support->setProvider("Simulated");
		support->setSimulatedResponses(CONTROLLED_MODE);
	}
	auto* institutions = new LLMBehavior(model, "Institutions");
	institutions->setAISupport(support);
	institutions->setPromptTemplate(
		"Responda com um único objeto JSON e nada mais: {\"action\": \"publish\", \"text\": \"...\"} para publicar uma manifestação "
		"na consulta pública, {\"action\": \"send\", \"to\": \"agente\", \"text\": \"...\"} para enviar uma mensagem privada a um dos "
		"agentes acima, ou {\"action\": \"none\"} para não agir. Escreva os textos em português, com no máximo duas frases.");

	auto* network = new AgentNetwork(model, "Consultation");
	const std::map<std::string, std::string> personas{
		{"Ministerio", "Ministério do Trabalho, autor do projeto. Defende a redução da jornada para proteger a saúde dos trabalhadores e busca acordo com as demais instituições."},
		{"Sindicato", "Sindicato dos trabalhadores. Apoia a redução da jornada, mas não aceita nenhuma redução de salário."},
		{"Associacao", "Associação das pequenas e médias empresas. Teme o aumento dos custos e só apoia a lei com alguma compensação."}};
	for (const auto& [name, persona] : personas) {
		auto* agent = new AgentNode(model, name);
		agent->setBehavior(institutions);
		auto* world = new LLMWorldModel(model);
		world->setPersona(persona);
		agent->setWorldModel(world);
		network->addNode(agent);
	}
	for (const std::string other : {"Sindicato", "Associacao"}) {
		for (const auto& [source, destination] : {std::pair{std::string("Ministerio"), other}, std::pair{other, std::string("Ministerio")}}) {
			auto* channel = new AgentChannel(model, source + "_" + destination);
			channel->setSource(network->findNodeByName(source));
			channel->setDestination(network->findNodeByName(destination));
			channel->setAcceptedMessageTypes({Util::TypeOf<TextMessage>()});
			network->addEdge(channel);
		}
	}
	auto* consultation = new PublicConsultationEnvironment(model);
	consultation->setLawText("Projeto de lei que reduz a jornada de trabalho de 44 para 40 horas semanais, sem redução de salário.");
	network->setEnvironment(consultation);

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
	model->getSimulation()->setNumberOfReplications(1);
	model->getSimulation()->setShowReportsAfterReplication(false);

	std::cout << "Public consultation in " << (demo ? "demonstration" : "controlled") << " mode with provider=" << support->getProvider()
			  << ", model=" << support->getModel() << std::endl;
	model->getSimulation()->start();
	delete genesys;
	return 0;
}
