#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace upf::gtp {

struct Header {
    std::uint8_t  flags;
    std::uint8_t  msg_type;
    std::uint16_t length;
    std::uint32_t teid;
};

constexpr std::size_t  HEADER_SIZE = 8;
constexpr std::uint8_t MSG_TYPE_G_PDU = 0xFF;
constexpr std::uint8_t FLAGS_G_PDU = 0x30;

inline std::array<std::byte, 2> write_be16(std::uint16_t v) noexcept {
    return {
        std::byte{static_cast<std::uint8_t>(v >> 8)},
        std::byte{static_cast<std::uint8_t>(v)}
    };
}

inline std::array<std::byte, 4> write_be32(std::uint32_t v) noexcept {
    return {
        std::byte{static_cast<std::uint8_t>(v >> 24)},
        std::byte{static_cast<std::uint8_t>(v >> 16)},
        std::byte{static_cast<std::uint8_t>(v >> 8)},
        std::byte{static_cast<std::uint8_t>(v)}
    };
}

inline std::uint16_t read_be16(std::span<const std::byte> in) noexcept {
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(in[0]) << 8) |
         std::to_integer<std::uint16_t>(in[1]));
}

inline std::uint32_t read_be32(std::span<const std::byte> in) noexcept {
    return (std::to_integer<std::uint32_t>(in[0]) << 24) |
           (std::to_integer<std::uint32_t>(in[1]) << 16) |
           (std::to_integer<std::uint32_t>(in[2]) << 8)  |
            std::to_integer<std::uint32_t>(in[3]);
}

inline std::array<std::byte, HEADER_SIZE>
serialize(const Header& h) noexcept {
    std::array<std::byte, HEADER_SIZE> out{};
    out[0] = std::byte{h.flags};
    out[1] = std::byte{h.msg_type};

    const auto len_bytes  = write_be16(h.length);
    const auto teid_bytes = write_be32(h.teid);

    std::copy(len_bytes.begin(),  len_bytes.end(),  out.begin() + 2);
    std::copy(teid_bytes.begin(), teid_bytes.end(), out.begin() + 4);

    return out;
}

inline Header parse(std::span<const std::byte> in) noexcept {
    Header h{};
    h.flags = std::to_integer<std::uint8_t>(in[0]);
    h.msg_type = std::to_integer<std::uint8_t>(in[1]);
    h.length = read_be16(in.subspan(2, 2));
    h.teid = read_be32(in.subspan(4, 4));
    return h;
}

}
