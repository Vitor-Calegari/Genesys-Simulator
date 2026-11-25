#ifndef AGENTPERSISTENCE_H
#define AGENTPERSISTENCE_H

#include "kernel/simulator/Persistence.h"
#include "kernel/simulator/PluginManager.h"
#include "kernel/simulator/Simulator.h"
#include "kernel/simulator/model/Model.h"

#include <memory>
#include <string>

/*!
 * \brief Saves \p element inside the record of its owner, under \p prefix, with the type of its
 * subclass, as GraphNetwork does with its nodes and edges.
 */
inline void SaveNested(PersistenceRecord* fields, const std::string& prefix, ModelDataDefinition* element) {
	auto nested = std::unique_ptr<PersistenceRecord>(fields->newInstance());
	ModelDataDefinition::SaveInstance(nested.get(), element);
	for (auto it = nested->begin(); it != nested->end(); ++it) {
		PersistenceRecord::Entry entry = it->second;
		entry.first = prefix + it->first;
		fields->insert(entry);
	}
}

/*! \brief Loads an element saved by SaveNested() through the plugin of its type, or returns nullptr. */
inline ModelDataDefinition* LoadNested(Model* model, PersistenceRecord* fields, const std::string& prefix) {
	auto nested = std::unique_ptr<PersistenceRecord>(fields->newInstance());
	for (auto it = fields->begin(); it != fields->end(); ++it) {
		if (it->first.rfind(prefix, 0) == 0) {
			PersistenceRecord::Entry entry = it->second;
			entry.first = it->first.substr(prefix.size());
			nested->insert(entry);
		}
	}
	Plugin* plugin = model->getParentSimulator()->getPluginManager()->find(nested->loadField("typename", std::string("")));
	return plugin != nullptr ? plugin->loadNew(model, nested.get()) : nullptr;
}

#endif /* AGENTPERSISTENCE_H */
