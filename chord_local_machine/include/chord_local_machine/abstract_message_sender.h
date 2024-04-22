#ifndef CHORD_LOCAL_MACHINE_ABSTRACT_MESSAGE_SENDER_H
#define CHORD_LOCAL_MACHINE_ABSTRACT_MESSAGE_SENDER_H

struct AbstractMessage {
    virtual ~AbstractMessage() = default;
};

template<class MessageType>
class AbstractMessageSender {
public:
    virtual ~AbstractMessageSender() = default;

    virtual void sendMessage(MessageType *message) = 0;
};

#endif // CHORD_LOCAL_MACHINE_ABSTRACT_MESSAGE_SENDER_H
