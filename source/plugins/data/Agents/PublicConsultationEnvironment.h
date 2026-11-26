#ifndef PUBLICCONSULTATIONENVIRONMENT_H
#define PUBLICCONSULTATIONENVIRONMENT_H

#include "AgentEnvironment.h"
#include "kernel/simulator/PluginInformation.h"

#include <memory>
#include <string>
#include <vector>

/*!
 * \brief Public consultation on a law. Every agent perceives the text of the law and the
 * statements already published, and the statements of a round are perceived from the next one.
 */
class PublicConsultationEnvironment : public AgentEnvironment {
public:
	PublicConsultationEnvironment(Model* model, std::string name = "");
	virtual ~PublicConsultationEnvironment() override = default;

public:
	static PluginInformation* GetPluginInformation();
	static ModelDataDefinition* LoadInstance(Model* model, PersistenceRecord* fields);
	static ModelDataDefinition* NewInstance(Model* model, std::string name = "");

public:
	virtual std::vector<std::shared_ptr<AgentMessage>> perceive(AgentNode* agent) const override;
	virtual void apply(const std::vector<std::shared_ptr<AgentMessage>>& messages) override;
	virtual std::vector<std::string> acceptedMessageTypes() const override;

public:
	void setLawText(const std::string& text);
	std::string getLawText() const;
	const std::vector<std::shared_ptr<AgentMessage>>& getStatements() const;

protected:
	virtual bool _loadInstance(PersistenceRecord* fields) override;
	virtual void _saveInstance(PersistenceRecord* fields, bool saveDefaultValues) override;
	virtual void _initBetweenReplications() override;

private:
	std::string _lawText;
	std::vector<std::shared_ptr<AgentMessage>> _statements;
};

#endif /* PUBLICCONSULTATIONENVIRONMENT_H */
