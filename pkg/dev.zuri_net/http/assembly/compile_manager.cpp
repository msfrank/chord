
#include <lyric_assembler/call_symbol.h>
#include <lyric_assembler/class_symbol.h>
#include <lyric_assembler/fundamental_cache.h>
#include <lyric_assembler/import_cache.h>
#include <lyric_assembler/local_variable.h>
#include <lyric_assembler/pack_builder.h>
#include <lyric_assembler/proc_handle.h>
#include <lyric_assembler/symbol_cache.h>
#include <lyric_assembler/template_handle.h>
#include <lyric_assembler/type_cache.h>
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
    auto *typeCache = state->typeCache();

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
    lyric_assembler::ClassSymbol *FutureClass;
    TU_ASSIGN_OR_RETURN (FutureClass, importCache->importClass(lyric_common::SymbolUrl(zuriStdSystem, futurePath)));

    // declare the chord http Manager class
    auto declareManagerClassResult = block->declareClass(
        "Manager", ObjectClass, lyric_object::AccessType::Public, {}, lyric_object::DeriveType::Final);
    if (declareManagerClassResult.isStatus())
        return declareManagerClassResult.getStatus();
    auto *ManagerClass = cast_symbol_to_class(
        symbolCache->getOrImportSymbol(declareManagerClassResult.getResult()).orElseThrow());

    auto UrlType = fundamentalCache->getFundamentalType(lyric_assembler::FundamentalSymbol::Url);
    auto ResponseType = lyric_common::TypeDef::forConcrete(lyric_common::SymbolUrl::fromString("#Response"));

    lyric_assembler::TypeHandle *futureOfResponseHandle;
    TU_ASSIGN_OR_RETURN (futureOfResponseHandle, typeCache->declareParameterizedType(
        FutureClass->getSymbolUrl(), {ResponseType}));
    auto FutureOfResponseType = futureOfResponseHandle->getTypeDef();

    {
        lyric_assembler::CallSymbol *callSymbol;
        TU_ASSIGN_OR_RETURN (callSymbol, ManagerClass->declareCtor(
            lyric_object::AccessType::Public, static_cast<tu_uint32>(NetHttpTrap::MANAGER_ALLOC)));
        lyric_assembler::ProcHandle *procHandle;
        TU_ASSIGN_OR_RETURN (procHandle, callSymbol->defineCall({}, lyric_common::TypeDef::noReturn()));
        auto *codeBuilder = procHandle->procCode();
        codeBuilder->trap(static_cast<tu_uint32>(NetHttpTrap::MANAGER_CTOR));
        codeBuilder->writeOpcode(lyric_object::Opcode::OP_RETURN);
    }
    {
        lyric_assembler::CallSymbol *callSymbol;
        TU_ASSIGN_OR_RETURN (callSymbol, ManagerClass->declareMethod(
            "Get", lyric_object::AccessType::Public));
        lyric_assembler::PackBuilder packBuilder;
        TU_RETURN_IF_NOT_OK (packBuilder.appendListParameter("url", "", UrlType, false));
        lyric_assembler::ParameterPack parameterPack;
        TU_ASSIGN_OR_RETURN (parameterPack, packBuilder.toParameterPack());
        lyric_assembler::ProcHandle *procHandle;
        TU_ASSIGN_OR_RETURN (procHandle, callSymbol->defineCall(parameterPack, FutureOfResponseType));
        auto *codeBuilder = procHandle->procCode();

        // construct the future and assign it to a local
        moduleEntry.compileBlock(R"(
            val fut: Future[Response] = Future[Response]{}
        )", procHandle->procBlock());

        // push fut onto the top of the stack
        lyric_assembler::DataReference var;
        TU_ASSIGN_OR_RETURN (var, procHandle->procBlock()->resolveReference("fut"));
        auto *sym = symbolCache->getOrImportSymbol(var.symbolUrl).orElseThrow();
        TU_ASSERT (sym != nullptr);
        TU_ASSERT (sym->getSymbolType() == lyric_assembler::SymbolType::LOCAL);
        auto *fut = cast_symbol_to_local(sym);
        codeBuilder->loadLocal(fut->getOffset());

        // call trap
        codeBuilder->trap(static_cast<tu_uint32>(NetHttpTrap::MANAGER_GET));

        // fut is still on the stack, return it
        codeBuilder->writeOpcode(lyric_object::Opcode::OP_RETURN);
    }

    return {};
}
