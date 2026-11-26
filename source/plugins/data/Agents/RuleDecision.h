#ifndef RULEDECISION_H
#define RULEDECISION_H

#include "AgentDecision.h"

#include <charconv>
#include <map>
#include <string>

/*! \brief Writes values as "variable=value; ...", with the shortest text that keeps each value. */
inline std::string ShowRuleValues(const std::map<std::string, double>& values) {
	std::string text;
	for (const auto& [name, value] : values) {
		char number[32];
		text += (text.empty() ? "" : "; ") + name + "=" + std::string(number, std::to_chars(number, number + sizeof(number), value).ptr);
	}
	return text;
}

/*! \brief Values that the rules computed for one agent, written to its world model in apply(). */
class RuleDecision : public AgentDecision {
public:
	std::string show() const override { return ShowRuleValues(values); }

public:
	std::map<std::string, double> values;
};

#endif /* RULEDECISION_H */
