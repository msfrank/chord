#ifndef CHORD_PROTOCOL_ABSTRACT_PROTOCOL_WRITER_H
#define CHORD_PROTOCOL_ABSTRACT_PROTOCOL_WRITER_H

#include <absl/strings/string_view.h>

#include <tempo_utils/status.h>

namespace chord_protocol {

    class AbstractProtocolWriter {
    public:
        virtual ~AbstractProtocolWriter() = default;

        virtual tempo_utils::Status write(std::string_view message) = 0;
    };
}

#endif // CHORD_PROTOCOL_ABSTRACT_PROTOCOL_WRITER_H