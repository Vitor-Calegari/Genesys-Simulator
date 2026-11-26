#ifndef LLMDECISION_H
#define LLMDECISION_H

#include "AgentDecision.h"

#include <string>

/*!
 * \brief Decision of an LLM agent: the interpreted response, what the agent perceived,
 * including the private messages it received, and the tokens and latency of the call.
 */
class LLMDecision : public AgentDecision {
public:
	std::string show() const override { return response; }

	std::string response;
	std::string perceived;
	unsigned int tokens = 0;
	/*! \brief Wall-clock seconds of the call. */
	double latency = 0.0;
};

#endif /* LLMDECISION_H */
