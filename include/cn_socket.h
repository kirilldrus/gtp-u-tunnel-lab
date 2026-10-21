#pragma once
#include "socket_helpers.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace upf {

class CnSocket {
public:
    explicit CnSocket(const std::string& ifname);
    ~CnSocket();

    CnSocket(const CnSocket&) = delete;
    CnSocket& operator=(const CnSocket&) = delete;

    int fd() const noexcept { return fd_; }

    const detail::mac_address_t& mac() const noexcept { return mac_; }

    std::ptrdiff_t recv(std::span<std::byte> buf) noexcept;
    std::ptrdiff_t send(std::span<const std::byte> buf) noexcept;

private:
    static void set_nonblocking(int fd);
    int fd_ = -1;
    detail::mac_address_t mac_{};
};

} 
