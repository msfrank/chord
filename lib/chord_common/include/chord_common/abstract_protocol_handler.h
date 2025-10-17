#ifndef CHORD_COMMON_ABSTRACT_PROTOCOL_HANDLER_H
#define CHORD_COMMON_ABSTRACT_PROTOCOL_HANDLER_H

#include "abstract_protocol_writer.h"

namespace chord_common {

    class AbstractProtocolHandler {

    public:
        virtual ~AbstractProtocolHandler() = default;

        virtual bool isAttached() = 0;
        virtual tempo_utils::Status attach(AbstractProtocolWriter *writer) = 0;
        virtual tempo_utils::Status send(std::string_view message) = 0;
        virtual tempo_utils::Status handle(std::string_view message) = 0;
        virtual tempo_utils::Status detach() = 0;
    };
}

#endif // CHORD_COMMON_ABSTRACT_PROTOCOL_HANDLER_H