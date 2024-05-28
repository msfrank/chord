#ifndef ZURI_NET_HTTP_CURL_UTILS_H
#define ZURI_NET_HTTP_CURL_UTILS_H

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>
#include <curl/curl.h>
#include <uv.h>

#include <lyric_runtime/data_cell.h>
#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/url.h>
#include <tempo_utils/uuid.h>

#include "plugin.h"

class ManagerRef;
struct ManagerPrivate;

struct Request {
    CURL *easy;
    tempo_utils::UUID id;
    tempo_utils::Url url;
    curl_slist *requestHeaders;
    std::shared_ptr<tempo_utils::ImmutableBytes> requestEntity;
    uv_async_t *notifyCompleted;
    CURLcode curlCode;
    long responseCode;
    tempo_utils::BytesAppender responseHeaders;
    tempo_utils::BytesAppender responseEntity;
    ManagerPrivate *priv;
};

struct ManagerPrivate {
    CURLM *multi;
    uv_loop_t *loop;
    uv_timer_t timer;
    absl::flat_hash_map<tempo_utils::UUID, Request *> inflight;
    ManagerRef *manager;
    PluginData *pluginData;
};

struct SocketPrivate {
    uv_poll_t poll;
    curl_socket_t socket;
    CURL *easy;
    long timeout;
    ManagerPrivate *priv;
};

size_t headers_write_cb(char *buffer, size_t size, size_t nmemb, void *_request);

size_t entity_write_cb(char *buffer, size_t size, size_t nmemb, void *_request);

int update_timeout_cb(CURLM *multi, long timeoutMs, void *_priv);

int socket_notify_cb(CURL *easy, curl_socket_t socket, int action, void *_priv, void *_sock);

#endif // ZURI_NET_HTTP_CURL_UTILS_H
