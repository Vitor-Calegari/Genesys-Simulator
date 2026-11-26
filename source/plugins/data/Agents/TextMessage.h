#ifndef TEXTMESSAGE_H
#define TEXTMESSAGE_H

#include "AgentMessage.h"
#include "kernel/util/Util.h"

#include <string>
#include <utility>

/*!
 * \brief Message with a text. Sent to the environment, with no receiver, it is a published
 * statement. Sent to an agent through a channel, it is a private message.
 */
class TextMessage : public AgentMessage {
public:
	TextMessage(AgentNode* sender, AgentNode* receiver, std::string text)
		: AgentMessage(Util::TypeOf<TextMessage>(), sender, receiver), _text(std::move(text)) {}
	std::string getText() const { return _text; }
	std::string show() const override { return _text; }

private:
	std::string _text;
};

#endif /* TEXTMESSAGE_H */
