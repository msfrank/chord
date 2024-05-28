
#include <absl/strings/substitute.h>

#include <lyric_runtime/bytecode_interpreter.h>
#include <lyric_runtime/interpreter_state.h>
#include <tempo_utils/log_stream.h>

#include "manager_ref.h"
#include "plugin.h"

ManagerRef::ManagerRef(
    const lyric_runtime::VirtualTable *vtable,
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state,
    PluginData *pluginData)
    : lyric_runtime::BaseRef(vtable)
{


    // get the uv main loop from the system scheduler
    auto *scheduler = state->systemScheduler();
    m_priv.loop = scheduler->systemLoop();

    // initialize private data
    m_priv.multi = curl_multi_init();

    // set curl callbacks
    curl_multi_setopt(m_priv.multi, CURLMOPT_SOCKETFUNCTION, socket_notify_cb);
    curl_multi_setopt(m_priv.multi, CURLMOPT_SOCKETDATA, &m_priv);
    curl_multi_setopt(m_priv.multi, CURLMOPT_TIMERFUNCTION, update_timeout_cb);
    curl_multi_setopt(m_priv.multi, CURLMOPT_TIMERDATA, &m_priv);

    uv_timer_init(m_priv.loop, &m_priv.timer);
    m_priv.timer.data = &m_priv;

    m_priv.manager = this;
    m_priv.pluginData = pluginData;
}

ManagerRef::~ManagerRef()
{
}

lyric_runtime::DataCell
ManagerRef::getField(const lyric_runtime::DataCell &field) const
{
    return {};
}

lyric_runtime::DataCell
ManagerRef::setField(const lyric_runtime::DataCell &field, const lyric_runtime::DataCell &value)
{
    return {};
}

std::string
ManagerRef::toString() const
{
    return absl::Substitute("<$0: ManagerRef>", this);
}

void
ManagerRef::finalize()
{
//    uv_close((uv_handle_t *) &m_priv.notifyExit, nullptr);
//    uv_close((uv_handle_t *) &m_priv.notifyPending, nullptr);
    uv_close((uv_handle_t *) &m_priv.timer, nullptr);

    // FIXME: cleanup inflight requests
    curl_multi_cleanup(m_priv.multi);

//    for (auto &request : m_priv.pending) {
//        delete request;
//    }
}

void
ManagerRef::setMembersReachable()
{
}

void
ManagerRef::clearMembersReachable()
{
}

static void
on_async_complete(lyric_runtime::Promise *promise)
{
    auto *request = static_cast<Request *>(promise->getData());
    TU_LOG_INFO << "request " << request->id.toString() << " responded with status " << (int) request->responseCode;
    auto headersBytes = request->responseHeaders.finish();
    CurlHeaders headers;
    TU_ASSIGN_OR_RAISE (headers, CurlHeaders::fromString(
        std::string_view((const char *) headersBytes->getData(), headersBytes->getSize())));
    for (auto it = headers.headersBegin(); it != headers.headersEnd(); it++) {
        TU_LOG_INFO << "response header: " << it->first << " = " << it->second;
    }
}

static void
on_resolve_response(
    lyric_runtime::Promise *promise,
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state)
{
    auto *heapManager = state->heapManager();
    auto *subroutineManager = state->subroutineManager();
    auto *currentCoro = state->currentCoro();

    auto *request = static_cast<Request *>(promise->getData());

    TU_LOG_INFO << "adapt request " << request->id.toString();

    auto entity = request->responseEntity.finish();
    auto entityView = std::string_view((const char *) entity->getData(), entity->getSize());

    auto responseCreateDescriptor = request->priv->pluginData->responseCreateDescriptor;

    auto arg0 = lyric_runtime::DataCell((tu_int64) request->responseCode);
    auto arg1 = heapManager->allocateString(entityView);
    std::vector<lyric_runtime::DataCell> args{arg0, arg1};

    tempo_utils::Status status;
    if (!subroutineManager->callStatic(responseCreateDescriptor, args, currentCoro, status)) {
        TU_LOG_ERROR << "failed to create Response: " << status;
        status.andThrow();
    }

    auto runInterpreterResult = interp->runSubinterpreter();
    if (runInterpreterResult.isStatus()) {
        TU_LOG_ERROR << "failed to create Response: " << runInterpreterResult.getStatus();
        runInterpreterResult.getStatus().andThrow();
    }

    promise->complete(runInterpreterResult.getResult());
}

void
free_request(void *data)
{
    auto *request = (Request *) data;
    delete request;
}

tempo_utils::Result<std::shared_ptr<lyric_runtime::Promise>>
ManagerRef::makeGetRequest(
    lyric_runtime::InterpreterState *state,
    const tempo_utils::Url &httpUrl,
    const CurlHeaders &requestHeaders)
{
    auto *request = new Request();
    request->easy = curl_easy_init();
    request->id = tempo_utils::UUID::randomUUID();
    request->url = httpUrl;
    request->requestHeaders = nullptr;
    request->priv = &m_priv;

    // set private pointer
    curl_easy_setopt(request->easy, CURLOPT_PRIVATE, request);

    // set GET method
    curl_easy_setopt(request->easy, CURLOPT_HTTPGET, 1L);

    // set curl callbacks
    curl_easy_setopt(request->easy, CURLOPT_HEADERFUNCTION, headers_write_cb);
    curl_easy_setopt(request->easy, CURLOPT_HEADERDATA, request);
    curl_easy_setopt(request->easy, CURLOPT_WRITEFUNCTION, entity_write_cb);
    curl_easy_setopt(request->easy, CURLOPT_WRITEDATA, request);

    // set url
    auto httpUrlString = httpUrl.toString();
    curl_easy_setopt(request->easy, CURLOPT_URL, httpUrlString.c_str());

    // set user agent
    curl_easy_setopt(request->easy, CURLOPT_USERAGENT, m_useragent.c_str());

    // set request headers if specified
    if (!requestHeaders.isEmpty()) {
        request->requestHeaders = requestHeaders.makeCurlSList();
        curl_easy_setopt(request->easy, CURLOPT_HTTPHEADER, request->requestHeaders);
    }

    // add the request to the curl multi handle
    auto curlcode = curl_multi_add_handle(m_priv.multi, request->easy);
    if (curlcode != CURLM_OK) {
        TU_LOG_ERROR << "curl_multi_add_handle failed: " << curl_multi_strerror(curlcode);
        curl_easy_cleanup(request->easy);
        delete request;
        return lyric_runtime::InterpreterStatus::forCondition(
            lyric_runtime::InterpreterCondition::kRuntimeInvariant,
            "failed to create http request: {}", curl_multi_strerror(curlcode));
    }

    // create the promise which will be resolved when the request completes
    lyric_runtime::PromiseOptions options;
    options.adapt = on_resolve_response;
    options.release = free_request;
    options.data = request;
    auto promise = lyric_runtime::Promise::create(on_async_complete, options);

    // register async handle which will be notified when the request completes
    auto *scheduler = state->systemScheduler();
    scheduler->registerAsync(&request->notifyCompleted, promise);

    TU_LOG_INFO << "GET " << httpUrl << " (request " << request->id.toString() << ")";

    return promise;
}

static lyric_runtime::DataCell
find_response_create_descriptor(
    lyric_runtime::BytecodeSegment *segment,
    lyric_runtime::SegmentManager *segmentManager)
{
    auto object = segment->getObject().getObject();
    auto symbol = object.findSymbol(lyric_common::SymbolPath({"Response", "$create"}));
    TU_ASSERT (symbol.isValid());

    lyric_runtime::InterpreterStatus status;
    auto descriptor = segmentManager->resolveDescriptor(segment,
        symbol.getLinkageSection(), symbol.getLinkageIndex(), status);
    TU_ASSERT (descriptor.type == lyric_runtime::DataCellType::CALL);
    TU_ASSERT (descriptor.data.descriptor.assembly == segment->getSegmentIndex());
    return descriptor;
}

tempo_utils::Status
manager_alloc(
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state)
{
    auto *currentCoro = state->currentCoro();

    auto &frame = currentCoro->peekCall();
    const auto *vtable = frame.getVirtualTable();
    TU_ASSERT(vtable != nullptr);

    auto *segmentManager = state->segmentManager();
    auto *callSegment = segmentManager->getSegment(frame.getCallSegment());
    auto *data = (PluginData *) callSegment->getData();
    if (!data->responseCreateDescriptor.isValid()) {
        data->responseCreateDescriptor = find_response_create_descriptor(callSegment, segmentManager);
    }

    auto ref = state->heapManager()->allocateRef<ManagerRef>(vtable, interp, state, data);
    currentCoro->pushData(ref);

    return lyric_runtime::InterpreterStatus::ok();
}

tempo_utils::Status
manager_ctor(
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state)
{
    auto *currentCoro = state->currentCoro();

    auto &frame = currentCoro->peekCall();
    auto receiver = frame.getReceiver();
    TU_ASSERT(receiver.type == lyric_runtime::DataCellType::REF);

    return lyric_runtime::InterpreterStatus::ok();
}

tempo_utils::Status
manager_get(
    lyric_runtime::BytecodeInterpreter *interp,
    lyric_runtime::InterpreterState *state)
{
    auto *currentCoro = state->currentCoro();

    auto &frame = currentCoro->peekCall();

    auto receiver = frame.getReceiver();
    TU_ASSERT(receiver.type == lyric_runtime::DataCellType::REF);
    auto *manager = static_cast<ManagerRef *>(receiver.data.ref);
    TU_ASSERT (manager != nullptr);

    TU_ASSERT (frame.numArguments() == 1);
    const auto &arg0 = frame.getArgument(0);
    TU_ASSERT (arg0.type == lyric_runtime::DataCellType::URL);

    TU_ASSERT (frame.numLocals() == 1);
    const auto &local0 = frame.getLocal(0);
    TU_ASSERT (local0.type == lyric_runtime::DataCellType::REF);
    auto *fut = local0.data.ref;

    tempo_utils::Url httpUrl;
    if (!arg0.data.ref->uriValue(httpUrl))
        return lyric_runtime::InterpreterStatus::forCondition(
            lyric_runtime::InterpreterCondition::kRuntimeInvariant, "failed to load http url");
    TU_ASSERT (httpUrl.isValid());

    std::shared_ptr<lyric_runtime::Promise> promise;
    TU_ASSIGN_OR_RETURN (promise, manager->makeGetRequest(state, httpUrl));
    fut->prepareFuture(promise);

    return lyric_runtime::InterpreterStatus::ok();
}
