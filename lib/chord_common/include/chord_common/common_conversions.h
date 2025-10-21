#ifndef CHORD_COMMON_COMMON_CONVERSIONS_H
#define CHORD_COMMON_COMMON_CONVERSIONS_H

#include <tempo_config/abstract_converter.h>
#include <tempo_config/enum_conversions.h>

#include "transport_location.h"

namespace chord_common {

    class TransportTypeParser : public tempo_config::EnumTParser<TransportType> {
    public:
        explicit TransportTypeParser(TransportType defaultType)
            : EnumTParser({
            {"Unix", TransportType::Unix},
            {"Tcp4", TransportType::Tcp4}}, defaultType)
        {}
        TransportTypeParser() : TransportTypeParser(TransportType::Invalid)
        {}
    };

    class TransportLocationParser : public tempo_config::AbstractConverter<TransportLocation> {
    public:
        TransportLocationParser();
        TransportLocationParser(const TransportLocation &transportLocationDefault);

        tempo_utils::Status convertValue(
            const tempo_config::ConfigNode &node,
            TransportLocation &location) const override;

    private:
        Option<TransportLocation> m_default;
    };
}

#endif // CHORD_COMMON_COMMON_CONVERSIONS_H