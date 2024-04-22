#ifndef CHORD_SANDBOX_SANDBOX_TYPES_H
#define CHORD_SANDBOX_SANDBOX_TYPES_H

#include <filesystem>
#include <string>
#include <vector>

#include <uv.h>

#include <tempo_utils/url.h>

namespace chord_sandbox {

    struct PendingWrite {
        uv_write_t req;
        uv_buf_t buf;

        static PendingWrite *create(size_t size);
        static PendingWrite *createFromBytes(std::string_view bytes);
    };
}

#endif // CHORD_SANDBOX_SANDBOX_TYPES_H