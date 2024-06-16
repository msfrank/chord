
#include <lyric_assembler/call_symbol.h>
#include <lyric_assembler/fundamental_cache.h>
#include <lyric_assembler/import_cache.h>
#include <lyric_assembler/pack_builder.h>
#include <lyric_assembler/proc_handle.h>
#include <lyric_assembler/struct_symbol.h>
#include <lyric_assembler/symbol_cache.h>
#include <lyric_typing/callsite_reifier.h>
#include <zuri_net_http/lib_types.h>

#include "compile_manager.h"

tempo_utils::Status
build_net_http_Response(
    lyric_compiler::ModuleEntry &moduleEntry,
    lyric_assembler::BlockHandle *block)
{
    auto *state = moduleEntry.getState();
    auto *fundamentalCache = state->fundamentalCache();
    auto *typeSystem = moduleEntry.getTypeSystem();

    auto IntType = fundamentalCache->getFundamentalType(lyric_assembler::FundamentalSymbol::Int);
    auto StringType = fundamentalCache->getFundamentalType(lyric_assembler::FundamentalSymbol::String);
    auto ResponseType = lyric_common::TypeDef::forConcrete(lyric_common::SymbolUrl::fromString("#Response"));

    lyric_assembler::StructSymbol *ResponseStruct;
    TU_ASSIGN_OR_RETURN (ResponseStruct, moduleEntry.compileStruct(R"(
        defstruct Response {
            val StatusCode: Int
            val Entity: String = ""
        }
    )", block));

    {
        lyric_assembler::CallSymbol *callSymbol;
        TU_ASSIGN_OR_RETURN (callSymbol, block->declareFunction(
            "Response.$create", lyric_object::AccessType::Public, {}));
        lyric_assembler::PackBuilder packBuilder;
        packBuilder.appendListParameter("code", "", IntType, false);
        packBuilder.appendListParameter("entity", "", StringType, false);
        lyric_assembler::ParameterPack parameterPack;
        TU_ASSIGN_OR_RETURN (parameterPack, packBuilder.toParameterPack());
        lyric_assembler::ProcHandle *procHandle;
        TU_ASSIGN_OR_RETURN (procHandle, callSymbol->defineCall(parameterPack, ResponseType));
        auto *codeBuilder = procHandle->procCode();
        auto *createBlock = procHandle->procBlock();

        lyric_assembler::ConstructableInvoker invoker;
        TU_RETURN_IF_NOT_OK (ResponseStruct->prepareCtor(invoker));
        lyric_typing::CallsiteReifier ctorReifier(typeSystem);
        TU_RETURN_IF_NOT_OK (ctorReifier.initialize(invoker));

        TU_RETURN_IF_NOT_OK (codeBuilder->loadArgument(lyric_assembler::ArgumentOffset(0)));
        TU_RETURN_IF_NOT_OK (ctorReifier.reifyNextArgument(IntType));
        TU_RETURN_IF_NOT_OK (codeBuilder->loadArgument(lyric_assembler::ArgumentOffset(1)));
        TU_RETURN_IF_NOT_OK (ctorReifier.reifyNextArgument(StringType));
        TU_RETURN_IF_STATUS (invoker.invokeNew(createBlock, ctorReifier, 0));
        codeBuilder->writeOpcode(lyric_object::Opcode::OP_RETURN);
    }

    return {};
}
