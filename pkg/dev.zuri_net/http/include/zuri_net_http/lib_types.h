#ifndef ZURI_NET_HTTP_LIB_TYPES_H
#define ZURI_NET_HTTP_LIB_TYPES_H

#include <cstdint>

enum class NetHttpTrap : uint32_t {
    MANAGER_ALLOC,
    MANAGER_CTOR,
    MANAGER_GET,
    LAST_,
};

#endif // ZURI_NET_HTTP_LIB_TYPES_H
