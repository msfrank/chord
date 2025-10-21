
#include <chord_common/common_conversions.h>
#include <tempo_config/config_result.h>

chord_common::TransportLocationParser::TransportLocationParser()
{
}

chord_common::TransportLocationParser::TransportLocationParser(const TransportLocation &transportLocationDefault)
    : m_default(transportLocationDefault)
{
}

tempo_utils::Status
chord_common::TransportLocationParser::convertValue(
    const tempo_config::ConfigNode &node,
    TransportLocation &location) const
{

    if (node.isNil() && !m_default.isEmpty()) {
        location = m_default.getValue();
        return {};
    }
    if (node.getNodeType() != tempo_config::ConfigNodeType::kValue)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType,
            "transport location config must be a value");

    auto value = node.toValue().getValue();
    auto convertedLocation = TransportLocation::fromString(value);
    if (convertedLocation.isValid()) {
        location = std::move(convertedLocation);
        return {};
    }

    return tempo_config::ConfigStatus::forCondition(
        tempo_config::ConfigCondition::kParseError,
        "value '{}' cannot be converted to TransportLocation", value);
}
