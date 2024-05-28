#ifndef ZURI_NET_HTTP_COMPILE_MANAGER_H
#define ZURI_NET_HTTP_COMPILE_MANAGER_H

#include <lyric_assembler/block_handle.h>
#include <lyric_compiler/module_entry.h>

tempo_utils::Status
build_net_http_Manager(
    lyric_compiler::ModuleEntry &moduleEntry,
    lyric_assembler::BlockHandle *block);

#endif // ZURI_NET_HTTP_COMPILE_MANAGER_H
