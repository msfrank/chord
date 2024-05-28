#ifndef ZURI_NET_HTTP_PLUGIN_H
#define ZURI_NET_HTTP_PLUGIN_H

#include <lyric_runtime/native_interface.h>

struct PluginData {
    lyric_runtime::DataCell responseCreateDescriptor;
};

class NetHttpPlugin : public lyric_runtime::NativeInterface {

public:
    NetHttpPlugin() = default;
    bool load(lyric_runtime::BytecodeSegment *segment) const override;
    void unload(lyric_runtime::BytecodeSegment *segment) const override;
    lyric_runtime::NativeFunc getTrap(uint32_t index) const override;
    uint32_t numTraps() const override;
};

#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MAC)

extern "C" const lyric_runtime::NativeInterface *native_init();

#elif defined(TARGET_OS_WINDOWS)

__declspec(dllexport) const lyric_runtime::NativeInterface *native_init();

#endif

#endif // ZURI_NET_HTTP_PLUGIN_H
