#ifndef ZURI_NET_HTTP_MANAGER_REF_H
#define ZURI_NET_HTTP_MANAGER_REF_H

#include <absl/container/flat_hash_map.h>
#include <curl/curl.h>
#include <uv.h>

#include <lyric_runtime/base_ref.h>

#include "curl_headers.h"
#include "curl_utils.h"
#include "plugin.h"

class ManagerRef : public lyric_runtime::BaseRef {

public:
    ManagerRef(
        const lyric_runtime::VirtualTable *vtable,
        lyric_runtime::BytecodeInterpreter *interp,
        lyric_runtime::InterpreterState *state,
        PluginData *pluginData);
    ~ManagerRef() override;

    lyric_runtime::DataCell getField(const lyric_runtime::DataCell &field) const override;
    lyric_runtime::DataCell setField(
        const lyric_runtime::DataCell &field,
        const lyric_runtime::DataCell &value) override;
    std::string toString() const override;
    void finalize() override;

    tempo_utils::Result<std::shared_ptr<lyric_runtime::Promise>> makeGetRequest(
        lyric_runtime::InterpreterState *state,
        const tempo_utils::Url &httpUrl,
        const CurlHeaders &requestHeaders = {});

protected:
    void setMembersReachable() override;
    void clearMembersReachable() override;

private:
    std::string m_useragent;
    ManagerPrivate m_priv;
};

tempo_utils::Status manager_alloc(
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state);

tempo_utils::Status manager_ctor(
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state);

tempo_utils::Status manager_get(
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state);

#endif // ZURI_NET_HTTP_MANAGER_REF_H
