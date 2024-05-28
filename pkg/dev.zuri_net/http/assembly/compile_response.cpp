
#include <lyric_assembler/call_symbol.h>
#include <lyric_assembler/fundamental_cache.h>
#include <lyric_assembler/import_cache.h>
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
    auto *symbolCache = state->symbolCache();
    auto *typeSystem = moduleEntry.getTypeSystem();

    lyric_assembler::StructSymbol *ResponseStruct;
    TU_ASSIGN_OR_RETURN (ResponseStruct, moduleEntry.compileStruct(R"(
        defstruct Response {
            val StatusCode: Int
            val Entity: String = ""
        }
    )", block));

    {
        auto ResponseSpec = lyric_parser::Assignable::forSingular({"Response"});
        auto IntSpec = lyric_parser::Assignable::forSingular({"Int"});
        auto StringSpec = lyric_parser::Assignable::forSingular({"String"});

        auto declareFunctionResult = block->declareFunction("Response.$create",
            {
                {{}, "code", "", StringSpec, lyric_parser::BindingType::VALUE},
                {{}, "entity", "", IntSpec, lyric_parser::BindingType::VALUE},
            },
            {},
            {},
            ResponseSpec,
            lyric_object::AccessType::Public,
            {});
        if (declareFunctionResult.isStatus())
            return declareFunctionResult.getStatus();
        auto functionUrl = declareFunctionResult.getResult();
        auto *call = cast_symbol_to_call(symbolCache->getOrImportSymbol(functionUrl).orElseThrow());
        auto *proc = call->callProc();
        auto *code = proc->procCode();
        auto *createBlock = proc->procBlock();

        auto resolveCtorResult = ResponseStruct->resolveCtor();
        if (resolveCtorResult.isStatus())
            return resolveCtorResult.getStatus();
        auto ctor = resolveCtorResult.getResult();
        lyric_typing::CallsiteReifier ctorReifier(ctor.getParameters(), ctor.getRest(),
            ctor.getTemplateUrl(), ctor.getTemplateParameters(), {}, typeSystem);
        TU_RETURN_IF_NOT_OK (ctorReifier.initialize());

        TU_RETURN_IF_NOT_OK (code->loadArgument(lyric_assembler::ArgumentOffset(0)));
        TU_RETURN_IF_NOT_OK (ctorReifier.reifyNextArgument(call->getParameters().at(0).typeDef));
        TU_RETURN_IF_NOT_OK (code->loadArgument(lyric_assembler::ArgumentOffset(1)));
        TU_RETURN_IF_NOT_OK (ctorReifier.reifyNextArgument(call->getParameters().at(1).typeDef));
        auto invokeNewResult = ctor.invokeNew(createBlock, ctorReifier);
        if (invokeNewResult.isStatus())
            return invokeNewResult.getStatus();
        code->writeOpcode(lyric_object::Opcode::OP_RETURN);
    }

    return lyric_assembler::AssemblerStatus::ok();
}
