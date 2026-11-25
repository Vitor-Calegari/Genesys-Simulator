#ifndef MESSAGEBROKER_H
#define MESSAGEBROKER_H

#include "AgentMessage.h"

#include <map>
#include <memory>
#include <vector>

/*!
 * \brief Delivers the messages sent in a round at the next round, apart from the channels.
 *
 * Each agent has two boxes: the messages readable in the current round and the messages sent to
 * it in this round. At the end of the round the sent messages become readable and the read ones
 * are discarded, except for agents whose decision failed, which receive them again.
 */
class MessageBroker {
public:
	void send(const std::shared_ptr<AgentMessage>& message);
	std::vector<std::shared_ptr<AgentMessage>> inbox(AgentNode* agent) const;
	/*! \brief Keeps the messages read by \p agent readable in the next round. */
	void redeliver(AgentNode* agent);
	void endRound();
	void clear();

private:
	std::map<AgentNode*, std::vector<std::shared_ptr<AgentMessage>>> _readable;
	std::map<AgentNode*, std::vector<std::shared_ptr<AgentMessage>>> _sent;
};

#endif /* MESSAGEBROKER_H */
