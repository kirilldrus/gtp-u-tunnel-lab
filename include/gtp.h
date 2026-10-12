#pragma once
#include <cstdint>

namespace upf::gtp {

#pragma pack(push, 1)
struct Header {
    uint8_t  flags;
    uint8_t  msg_type;
    uint16_t length;
    uint32_t teid;
};
#pragma pack(pop)

static_assert(sizeof(Header) == 8, "GTP-U header must be 8 bytes");

}
