#include "PublicConsultationEnvironment.h"
#include "TextMessage.h"

#include "kernel/simulator/PluginInformation.h"
#include "kernel/simulator/model/Model.h"

#include <functional>

#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &PublicConsultationEnvironment::GetPluginInformation;
}
#endif

PublicConsultationEnvironment::PublicConsultationEnvironment(Model* model, std::string name)
	: AgentEnvironment(model, Util::TypeOf<PublicConsultationEnvironment>(), name) {
	auto* lawText = new SimulationControlString(
		std::bind(&PublicConsultationEnvironment::getLawText, this), std::bind(&PublicConsultationEnvironment::setLawText, this, std::placeholders::_1),
		Util::TypeOf<PublicConsultationEnvironment>(), getName(), "LawText", "Text of the law under consultation, perceived by every agent.");
	lawText->setPreferredEditorHint(SimulationControlEditorHint::MultiLineText);
	_parentModel->getControls()->insert(lawText);
	_addSimulationControl(lawText);
}

PluginInformation* PublicConsultationEnvironment::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<PublicConsultationEnvironment>(), &PublicConsultationEnvironment::LoadInstance, &PublicConsultationEnvironment::NewInstance);
	info->setCategory("Agents");
	info->setDescriptionHelp("Environment of a public consultation, with the text of the law and the published statements.");
	return info;
}

ModelDataDefinition* PublicConsultationEnvironment::LoadInstance(Model* model, PersistenceRecord* fields) {
	PublicConsultationEnvironment* environment = new PublicConsultationEnvironment(model);
	environment->_loadInstance(fields);
	return environment;
}

ModelDataDefinition* PublicConsultationEnvironment::NewInstance(Model* model, std::string name) {
	return new PublicConsultationEnvironment(model, name);
}

std::vector<std::shared_ptr<AgentMessage>> PublicConsultationEnvironment::perceive(AgentNode* agent) const {
	std::vector<std::shared_ptr<AgentMessage>> perceived{std::make_shared<TextMessage>(nullptr, agent, _lawText)};
	perceived.insert(perceived.end(), _statements.begin(), _statements.end());
	return perceived;
}

void PublicConsultationEnvironment::apply(const std::vector<std::shared_ptr<AgentMessage>>& messages) {
	_statements.insert(_statements.end(), messages.begin(), messages.end());
}

std::vector<std::string> PublicConsultationEnvironment::acceptedMessageTypes() const {
	return {Util::TypeOf<TextMessage>()};
}

void PublicConsultationEnvironment::setLawText(const std::string& text) {
	_lawText = text;
}

std::string PublicConsultationEnvironment::getLawText() const {
	return _lawText;
}

const std::vector<std::shared_ptr<AgentMessage>>& PublicConsultationEnvironment::getStatements() const {
	return _statements;
}

bool PublicConsultationEnvironment::_loadInstance(PersistenceRecord* fields) {
	bool res = AgentEnvironment::_loadInstance(fields);
	if (res) {
		_lawText = fields->loadField("lawText", std::string(""));
	}
	return res;
}

void PublicConsultationEnvironment::_saveInstance(PersistenceRecord* fields, bool saveDefaultValues) {
	AgentEnvironment::_saveInstance(fields, saveDefaultValues);
	fields->saveField("lawText", _lawText, std::string(""), saveDefaultValues);
}

void PublicConsultationEnvironment::_initBetweenReplications() {
	_statements.clear();
}
