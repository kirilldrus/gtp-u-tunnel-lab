#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <netinet/in.h>

namespace upf {

class AnSocket {
public:
    AnSocket(const std::string& bind_ip, std::uint16_t port);
    ~AnSocket();

    AnSocket(const AnSocket&) = delete;
    AnSocket& operator=(const AnSocket&) = delete;

    int fd() const noexcept { return fd_; }

    std::tuple<std::ptrdiff_t, sockaddr_in> recv(std::span<std::byte> buf) noexcept;
    std::ptrdiff_t send_to(std::span<const std::byte> buf,
                           const sockaddr_in& dst) noexcept;

private:
    static void set_nonblocking(int fd);
    int fd_ = -1;
};

} 
