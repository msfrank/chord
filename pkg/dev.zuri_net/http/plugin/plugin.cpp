
#include <zuri_net_http/lib_types.h>

#include "manager_ref.h"
#include "plugin.h"

lyric_runtime::NativeFunc
NetHttpPlugin::getTrap(uint32_t index) const
{
    if (index >= static_cast<uint32_t>(NetHttpTrap::LAST_))
        return nullptr;
    auto trapFunction = static_cast<NetHttpTrap>(index);
    switch (trapFunction) {
        case NetHttpTrap::MANAGER_ALLOC:
            return manager_alloc;
        case NetHttpTrap::MANAGER_CTOR:
            return manager_ctor;
        case NetHttpTrap::MANAGER_GET:
            return manager_get;
        case NetHttpTrap::LAST_:
            break;
    }
    TU_UNREACHABLE();
}

bool
NetHttpPlugin::load(lyric_runtime::BytecodeSegment *segment) const
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
        return false;
    auto *data = new PluginData;
    segment->setData(data);
    return  true;
}

void
NetHttpPlugin::unload(lyric_runtime::BytecodeSegment *segment) const
{
    auto *data = (PluginData *) segment->getData();
    delete data;
}

uint32_t
NetHttpPlugin::numTraps() const
{
    return static_cast<uint32_t>(NetHttpTrap::LAST_);
}

static const NetHttpPlugin iface;

const lyric_runtime::NativeInterface *native_init()
{
    return &iface;
}
