
#include <lyric_assembler/call_symbol.h>
#include <lyric_assembler/class_symbol.h>
#include <lyric_assembler/fundamental_cache.h>
#include <lyric_assembler/import_cache.h>
#include <lyric_assembler/local_variable.h>
#include <lyric_assembler/proc_handle.h>
#include <lyric_assembler/symbol_cache.h>
#include <zuri_net_http/lib_types.h>

#include "compile_manager.h"

tempo_utils::Status
build_net_http_Manager(
    lyric_compiler::ModuleEntry &moduleEntry,
    lyric_assembler::BlockHandle *block)
{
    auto *state = moduleEntry.getState();
    auto *fundamentalCache = state->fundamentalCache();
    auto *importCache = state->importCache();
    auto *symbolCache = state->symbolCache();

    // import Object class from the prelude
    lyric_assembler::ClassSymbol *ObjectClass = nullptr;
    TU_ASSIGN_OR_RETURN (ObjectClass, importCache->importClass(
        fundamentalCache->getFundamentalUrl(lyric_assembler::FundamentalSymbol::Object)));

    // import the std system module symbols into the block
    auto zuriStdSystemUrl = tempo_utils::Url::fromOrigin(ZURI_STD_PACKAGE_URL, "/system");
    auto zuriStdSystem = lyric_common::AssemblyLocation::fromUrl(zuriStdSystemUrl);
    TU_RETURN_IF_NOT_OK (importCache->importModule(zuriStdSystem, block));

    // import the Future class from the std system module
    lyric_common::SymbolPath futurePath({"Future"});
    TU_RETURN_IF_STATUS(importCache->importClass(lyric_common::SymbolUrl(zuriStdSystem, futurePath)));

    // declare the chord http Manager class
    auto declareManagerClassResult = block->declareClass("Manager", ObjectClass, lyric_object::AccessType::Public,
        {}, lyric_object::DeriveType::Final);
    if (declareManagerClassResult.isStatus())
        return declareManagerClassResult.getStatus();
    auto *ManagerClass = cast_symbol_to_class(
        symbolCache->getOrImportSymbol(declareManagerClassResult.getResult()).orElseThrow());

    auto UrlSpec = lyric_parser::Assignable::forSingular({"Url"});
    auto ResponseSpec = lyric_parser::Assignable::forSingular({"Response"});
    auto FutureOfResponseSpec = lyric_parser::Assignable::forSingular(zuriStdSystemUrl,
        futurePath, {ResponseSpec});

    {
        auto declareCtorResult = ManagerClass->declareCtor(
            {},
            {},
            {},
            lyric_object::AccessType::Public,
            static_cast<tu_uint32>(NetHttpTrap::MANAGER_ALLOC));
        auto *call = cast_symbol_to_call(symbolCache->getOrImportSymbol(declareCtorResult.getResult()).orElseThrow());
        auto *code = call->callProc()->procCode();
        code->trap(static_cast<tu_uint32>(NetHttpTrap::MANAGER_CTOR));
        code->writeOpcode(lyric_object::Opcode::OP_RETURN);
    }
    {
        auto declareMethodResult = ManagerClass->declareMethod("Get",
            {
                { {}, "url", "", UrlSpec, lyric_parser::BindingType::VALUE },
            },
            {},
            {},
            FutureOfResponseSpec,
            lyric_object::AccessType::Public);
        auto *call = cast_symbol_to_call(symbolCache->getOrImportSymbol(declareMethodResult.getResult()).orElseThrow());
        auto *proc = call->callProc();
        auto *code = proc->procCode();

        // construct the future and assign it to a local
        moduleEntry.compileBlock(R"(
            val fut: Future[Response] = Future[Response]{}
        )", proc->procBlock());

        // push fut onto the top of the stack
        lyric_assembler::DataReference var;
        TU_ASSIGN_OR_RETURN (var, proc->procBlock()->resolveReference("fut"));
        auto *sym = symbolCache->getOrImportSymbol(var.symbolUrl).orElseThrow();
        TU_ASSERT (sym != nullptr);
        TU_ASSERT (sym->getSymbolType() == lyric_assembler::SymbolType::LOCAL);
        auto *fut = cast_symbol_to_local(sym);
        code->loadLocal(fut->getOffset());

        // call trap
        code->trap(static_cast<tu_uint32>(NetHttpTrap::MANAGER_GET));

        // fut is still on the stack, return it
        code->writeOpcode(lyric_object::Opcode::OP_RETURN);
    }

    return lyric_assembler::AssemblerStatus::ok();
}
