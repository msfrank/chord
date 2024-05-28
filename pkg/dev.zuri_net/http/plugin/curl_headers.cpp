
#include <absl/strings/ascii.h>
#include <absl/strings/strip.h>
#include <absl/strings/str_split.h>

#include <lyric_runtime/interpreter_result.h>

#include "curl_headers.h"

CurlHeaders::CurlHeaders(const std::multimap<std::string,std::string> &headers)
    : m_headers(headers)
{
}

CurlHeaders::CurlHeaders(const CurlHeaders &other)
    : m_headers(other.m_headers)
{
}

inline std::string
normalize_header_name(std::string_view name)
{
    std::string normalized(name);
    absl::StripAsciiWhitespace(&normalized);
    absl::AsciiStrToLower(&normalized);
    return normalized;
}

inline std::string
normalize_header_value(std::string_view value)
{
    std::string normalized(value);
    absl::StripAsciiWhitespace(&normalized);
    return normalized;
}

inline std::string
normalize_header(std::string_view name, std::string_view value)
{
    return absl::StrCat(normalize_header_name(name), ": ", normalize_header_value(value));
}

bool
CurlHeaders::isEmpty() const
{
    return m_headers.empty();
}

int
CurlHeaders::size() const
{
    return m_headers.size();
}

bool
CurlHeaders::hasHeader(std::string_view header) const
{
    auto iterator = m_headers.find(normalize_header_name(header));
    return iterator != m_headers.end();
}

int
CurlHeaders::numHeaderValues(std::string_view header) const
{
    return m_headers.count(normalize_header_name(header));
}

Option<std::string>
CurlHeaders::getFirstHeader(std::string_view header) const
{
    auto name = normalize_header_name(header);
    auto range = m_headers.equal_range(name);
    if (range.first == m_headers.end())
        return {};
    return Option<std::string>(normalize_header(name, range.first->second));
}

Option<std::string>
CurlHeaders::getFirstHeaderValue(std::string_view header) const
{
    auto name = normalize_header_name(header);
    auto range = m_headers.equal_range(name);
    if (range.first == m_headers.end())
        return {};
    return Option<std::string>(normalize_header_value(range.first->second));
}

std::vector<std::string>
CurlHeaders::getHeader(std::string_view header) const
{
    std::vector<std::string> headers;

    auto name = normalize_header_name(header);
    auto range = m_headers.equal_range(name);
    if (range.first == m_headers.end())
        return {};
    for (auto iterator = range.first; iterator != range.second; range.first++) {
        headers.push_back(normalize_header(name, iterator->second));
    }
    return headers;
}

std::vector<std::string>
CurlHeaders::getHeaderValue(std::string_view header) const
{
    std::vector<std::string> values;

    auto name = normalize_header_name(header);
    auto range = m_headers.equal_range(name);
    if (range.first == m_headers.end())
        return {};
    for (auto iterator = range.first; iterator != range.second; range.first++) {
        values.push_back(normalize_header_value(iterator->second));
    }
    return values;
}

void
CurlHeaders::setHeader(std::string_view header, std::string_view value)
{
    auto name = normalize_header_name(header);
    m_headers.erase(name);
    m_headers.emplace(name, normalize_header_value(value));
}

void
CurlHeaders::appendHeader(std::string_view header, std::string_view value)
{
    auto name = normalize_header_name(header);
    m_headers.emplace(name, normalize_header_value(value));
}

absl::flat_hash_set<std::string>
CurlHeaders::listHeaderNames() const
{
    absl::flat_hash_set<std::string> names;
    for (auto &entry : m_headers) {
        names.insert(entry.first);
    }
    return names;
}

std::multimap<std::string,std::string>::const_iterator
CurlHeaders::headersBegin() const
{
    return m_headers.cbegin();
}

std::multimap<std::string,std::string>::const_iterator
CurlHeaders::headersEnd() const
{
    return m_headers.cend();
}

curl_slist *
CurlHeaders::makeCurlSList() const
{
    curl_slist *headers = nullptr;
    for (auto &entry : m_headers) {
        auto header = normalize_header(entry.first, entry.second);
        headers = curl_slist_append(headers, header.c_str());
    }
    return headers;
}

tempo_utils::Result<CurlHeaders>
CurlHeaders::fromString(std::string_view bytes)
{
    // split UTF8 header data into individual header lines, and coalesce continuations
    std::multimap<std::string,std::string> headers;
    std::string curr;

    auto lines = absl::StrSplit(bytes, absl::ByString("\n"));

    auto iterator = lines.begin();
    if (iterator == lines.end())
        return CurlHeaders();

    // skip the first line, which is the status line
    iterator++;

    // iterate over the remaining lines
    for (; iterator != lines.end(); iterator++) {
        const auto &line = *iterator;

        // ignore empty line
        if (line.empty())
            continue;

        // if line is a continuation, then append it to the current header
        if (absl::ascii_isspace(line[0]) && !curr.empty()) {
            auto continuation = normalize_header_value(line);
            curr.append(" ");
            curr.append(continuation);
            continue;
        }

        // if we have an entire header, then add it to the header list
        if (!curr.empty()) {
            int index = curr.find_first_of(':');
            if (index == std::string::npos)
                return lyric_runtime::InterpreterStatus::forCondition(
                    lyric_runtime::InterpreterCondition::kRuntimeInvariant, "invalid header");
            auto name = normalize_header_name(curr.substr(0, index));
            auto value = normalize_header_value(curr.substr(index + 1));
            headers.emplace(name, value);
        }
        // start tracking a new header
        curr = line;
    }

    return CurlHeaders(headers);
}
