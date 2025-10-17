#ifndef CHORD_COMMON_ABSTRACT_PROTOCOL_WRITER_H
#define CHORD_COMMON_ABSTRACT_PROTOCOL_WRITER_H

#include <absl/strings/string_view.h>

#include <tempo_utils/status.h>

namespace chord_common {

    class AbstractProtocolWriter {
    public:
        virtual ~AbstractProtocolWriter() = default;

        virtual tempo_utils::Status write(std::string_view message) = 0;
    };
}

#endif // CHORD_COMMON_ABSTRACT_PROTOCOL_WRITER_H