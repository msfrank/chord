
#include <lyric_bootstrap/bootstrap_loader.h>
#include <lyric_compiler/module_entry.h>
#include <lyric_packaging/package_loader.h>
#include <lyric_runtime/chain_loader.h>
#include <tempo_command/command_help.h>
#include <tempo_utils/file_writer.h>
#include <tempo_utils/log_stream.h>

#include "compile_manager.h"
#include "compile_response.h"

tempo_utils::Status
build(int argc, char *argv[])
{
    std::filesystem::path destinationPath;
    if (argc < 2) {
        destinationPath = DEFAULT_DESTINATION_PATH;
    } else {
        destinationPath = argv[1];
    }

    auto recorder = tempo_tracing::TraceRecorder::create();
    tempo_tracing::ScopeManager scopeManager(recorder);
    auto span = scopeManager.makeSpan();
    span->setOperationName("buildChordHttp");

    auto location = lyric_common::AssemblyLocation::fromString("/http");

    // build the loader chain
    std::vector<std::shared_ptr<lyric_runtime::AbstractLoader>> loaderChain;
    loaderChain.push_back(std::make_shared<lyric_bootstrap::BootstrapLoader>());
    loaderChain.push_back(std::shared_ptr<lyric_packaging::PackageLoader>(
        new lyric_packaging::PackageLoader({ZURI_INSTALL_PACKAGES_DIR})));
    loaderChain.push_back(std::shared_ptr<lyric_packaging::PackageLoader>(
        new lyric_packaging::PackageLoader({CHORD_BUILD_PACKAGES_DIR})));
    auto loader = std::make_shared<lyric_runtime::ChainLoader>(loaderChain);

    auto sharedModuleCache = lyric_importer::ModuleCache::create(loader);

    lyric_assembler::AssemblyState assemblyState(location, sharedModuleCache, &scopeManager);

    // initialize the assembler
    TU_RETURN_IF_NOT_OK (assemblyState.initialize());

    // define the module entry point
    lyric_compiler::ModuleEntry moduleEntry(&assemblyState);
    TU_RETURN_IF_NOT_OK (moduleEntry.initialize());

    auto *root = moduleEntry.getRoot();
    auto *rootBlock = root->namespaceBlock();

    TU_RETURN_IF_NOT_OK(build_net_http_Response(moduleEntry, rootBlock));
    TU_RETURN_IF_NOT_OK(build_net_http_Manager(moduleEntry, rootBlock));

    // serialize state to object
    lyric_object::LyricObject object;
    TU_ASSIGN_OR_RETURN (object, assemblyState.toAssembly());

    // write object to file
    tempo_utils::FileWriter writer(destinationPath, object.bytesView(),
        tempo_utils::FileWriterMode::CREATE_OR_OVERWRITE);
    if (!writer.isValid()) {
        return writer.getStatus();
    }

    TU_LOG_INFO << "wrote output to " << destinationPath;
    return {};
}

int
main(int argc, char *argv[])
{
    auto status = build(argc, argv);
    if (!status.isOk())
        tempo_command::display_status_and_exit(status);
    return 0;
}
