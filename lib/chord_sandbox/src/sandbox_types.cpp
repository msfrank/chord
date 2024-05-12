
#include <chord_sandbox/sandbox_types.h>
#include <tempo_utils/log_message.h>

chord_sandbox::PendingWrite *
chord_sandbox::PendingWrite::create(size_t size)
{
    auto *write = (PendingWrite *) malloc(sizeof(PendingWrite) + size);
    TU_ASSERT (write != nullptr);
    write->buf.base = ((char *) write) + sizeof(PendingWrite);
    write->buf.len = size;
    return write;
}

chord_sandbox::PendingWrite *
chord_sandbox::PendingWrite::createFromBytes(std::string_view bytes)
{
    auto *write = create(bytes.size());
    memcpy(write->buf.base, bytes.data(), bytes.size());
    return write;
}