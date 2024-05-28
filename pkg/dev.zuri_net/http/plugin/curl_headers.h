#ifndef ZURI_NET_HTTP_CURL_HEADERS_H
#define ZURI_NET_HTTP_CURL_HEADERS_H

#include <string>
#include <map>

#include <absl/container/flat_hash_set.h>
#include <curl/curl.h>

#include <tempo_utils/option_template.h>
#include <tempo_utils/result.h>

class CurlHeaders {

public:
    CurlHeaders() = default;
    explicit CurlHeaders(const std::multimap<std::string,std::string> &headers);
    CurlHeaders(const CurlHeaders &other);

    bool isEmpty() const;
    int size() const;

    bool hasHeader(std::string_view header) const;
    int numHeaderValues(std::string_view header) const;
    Option<std::string> getFirstHeader(std::string_view header) const;
    Option<std::string> getFirstHeaderValue(std::string_view header) const;
    std::vector<std::string> getHeader(std::string_view header) const;
    std::vector<std::string> getHeaderValue(std::string_view header) const;
    void setHeader(std::string_view header, std::string_view value);
    void appendHeader(std::string_view header, std::string_view value);
    absl::flat_hash_set<std::string> listHeaderNames() const;

    std::multimap<std::string,std::string>::const_iterator headersBegin() const;
    std::multimap<std::string,std::string>::const_iterator headersEnd() const;

    curl_slist *makeCurlSList() const;

    static tempo_utils::Result<CurlHeaders> fromString(std::string_view string);

private:
    std::multimap<std::string, std::string> m_headers;
};

#endif // ZURI_NET_HTTP_CURL_HEADERS_H
