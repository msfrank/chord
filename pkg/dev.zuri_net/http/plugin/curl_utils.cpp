
#include "curl_utils.h"

/**
 * when signaled by curl this callback gets invoked as soon as it has received header data.
 * the header callback is called once for each header and only complete header lines are passed
 * on to the callback.
 *
 * @param buffer
 * @param size
 * @param nmemb
 * @param _request
 * @return
 */
size_t
headers_write_cb(char *buffer, size_t size, size_t nmemb, void *_request)
{
    Request *request = (Request *) _request;

    size_t realsize = size * nmemb;
    request->responseHeaders.appendBytes(std::string_view(buffer, realsize));
    TU_LOG_INFO << "received header data (" << (int) realsize << " bytes)";
    return realsize;
}

/**
 * when signaled by curl this callback gets called as soon as there is data received that
 * needs to be saved.
 *
 * @param buffer
 * @param size
 * @param nmemb
 * @param _request
 * @return
 */
size_t
entity_write_cb(char *buffer, size_t size, size_t nmemb, void *_request)
{
    Request *request = (Request *) _request;

    size_t realsize = size * nmemb;
    request->responseEntity.appendBytes(std::string_view(buffer, realsize));
    TU_LOG_INFO << "received entity data (" << (int) realsize << " bytes)";
    return realsize;
}

static void
remove_completed(ManagerPrivate *priv)
{
    int msgs_left;
    CURLMsg *msg;

    while ((msg = curl_multi_info_read(priv->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy = msg->easy_handle;
            Request *request;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &request);

            request->curlCode = msg->data.result;
            if (request->curlCode == CURLE_OK) {
                curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &request->responseCode);
                uv_async_send(request->notifyCompleted);
            } else {
                TU_LOG_V << "curl easy handle failure: " << curl_easy_strerror(request->curlCode);
                uv_async_send(request->notifyCompleted);
            }

            curl_multi_remove_handle(priv->multi, easy);
            curl_easy_cleanup(easy);
        }
    }
}

/**
 * call either curl_multi_socket_action or curl_multi_perform, depending on which interface you use.
 *
 * @param handle
 */
static void
timer_expires_cb(uv_timer_t *handle)
{
    ManagerPrivate *priv = (ManagerPrivate *) handle->data;
    int stillRunning;

    auto curlcode = curl_multi_socket_action(priv->multi, CURL_SOCKET_TIMEOUT, 0, &stillRunning);
    if (curlcode != CURLM_OK) {
        TU_LOG_ERROR << "curl_multi_socket_action failed: " << curl_multi_strerror(curlcode);
    }
    remove_completed(priv);
}

/**
 * when signaled by curl this callback installs a non-repeating timer with an expire time
 * of timeout_ms milliseconds. when the timer fires it invokes timer_expires_cb.
 *
 * @param multi
 * @param timeoutMs
 * @param _priv
 * @return
 */
int
update_timeout_cb(CURLM *multi, long timeoutMs, void *_priv)
{
    ManagerPrivate *priv = (ManagerPrivate *) _priv;


    if (timeoutMs < 0) {
        TU_LOG_INFO << "update_timeout_cb: stopped timer";
        uv_timer_stop(&priv->timer);
    } else {
        if (timeoutMs == 0) {
            timeoutMs = 1;  // set a very short delay
        }
        TU_LOG_INFO << "update_timeout_cb: reset timer for " << (int) timeoutMs << "ms";
        uv_timer_start(&priv->timer, timer_expires_cb, timeoutMs, 0);
    }
    return 0;
}

/**
 * when signaled by uv this callback notifies curl of poll events on the socket.
 *
 * @param poll
 * @param status
 * @param events
 */
static void
poll_event_cb(uv_poll_t *poll, int status, int events)
{
    SocketPrivate *sock = (SocketPrivate *) poll->data;
    ManagerPrivate *priv = (ManagerPrivate *) sock->priv;

    if (status == 0) {
        int action = 0, stillRunning;
        action |= (events & UV_READABLE ? CURL_CSELECT_IN : 0);
        action |= (events & UV_WRITABLE ? CURL_CSELECT_OUT : 0);
        auto curlcode = curl_multi_socket_action(priv->multi, sock->socket, action, &stillRunning);
        if (curlcode != CURLM_OK) {
            TU_LOG_ERROR << "curl_multi_socket_action failed: " << curl_multi_strerror(curlcode);
        }
        remove_completed(priv);

        if (stillRunning <= 0) {
            uv_timer_stop(&priv->timer);
        }
    }
}

static SocketPrivate *
create_socket_private(
    CURL *easy,
    curl_socket_t socket,
    ManagerPrivate *priv)
{
    auto *sock = new SocketPrivate();
    uv_poll_init_socket(priv->loop, &sock->poll, socket);
    sock->poll.data = sock;
    sock->socket = socket;
    sock->easy = easy;
    //sock->action = action;
    sock->timeout = 0;
    sock->priv = priv;
    TU_LOG_INFO << "created socket " << sock;
    return sock;
}

static void
delete_socket_private(uv_handle_t *handle)
{
    SocketPrivate *sock = (SocketPrivate *) handle->data;
    delete sock;
}

/**
 * when signaled by curl this callback informs the application about updates in the
 * socket (file descriptor) status.
 *
 * @param easy
 * @param socket
 * @param action
 * @param _priv
 * @param _sock
 * @return
 */
int
socket_notify_cb(
    CURL *easy,
    curl_socket_t socket,
    int action,
    void *_priv,
    void *_sock)
{
    ManagerPrivate *priv = (ManagerPrivate *) _priv;
    TU_ASSERT (priv != nullptr);

    TU_LOG_INFO << "socket_notify_cb";

    SocketPrivate *sock = (SocketPrivate *) _sock;

    switch (action) {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT: {
            if (sock == nullptr) {
                sock = create_socket_private(easy, socket, priv);
            }
            curl_multi_assign(priv->multi, socket, sock);
            int events = 0;
            if(action != CURL_POLL_IN)
                events |= UV_WRITABLE;
            if(action != CURL_POLL_OUT)
                events |= UV_READABLE;
            uv_poll_start(&sock->poll, events, poll_event_cb);
            break;
        }
        case CURL_POLL_REMOVE: {
            if (sock) {
                TU_LOG_INFO << "removed socket " << sock;
                uv_poll_stop(&sock->poll);
                uv_close((uv_handle_t *) &sock->poll, delete_socket_private);
                curl_multi_assign(priv->multi, socket, nullptr);
            }
            break;
        }
        default:
            break;
    }

    return 0;
}
