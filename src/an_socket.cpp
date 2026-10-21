#include "an_socket.h"
#include "socket_helpers.h"
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace upf {

void AnSocket::set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL failed");
}

AnSocket::AnSocket(const std::string& bind_ip, std::uint16_t port) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) throw std::runtime_error("Failed to create UDP socket");

    auto addr = detail::make_udp_addr(bind_ip, port);
    if (detail::bind_socket(fd_, addr) < 0) {
        ::close(fd_);
        throw std::runtime_error("Failed to bind UDP socket");
    }

    set_nonblocking(fd_);
}

AnSocket::~AnSocket() {
    if (fd_ >= 0) ::close(fd_);
}

std::tuple<std::ptrdiff_t, sockaddr_in> 
AnSocket::recv(std::span<std::byte> buf) noexcept {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    const auto size = ::recvfrom(fd_, buf.data(), buf.size(), 0,
                                 reinterpret_cast<sockaddr*>(&peer), &peer_len);
    return {size, peer};
}

std::ptrdiff_t AnSocket::send_to(std::span<const std::byte> buf,
                                 const sockaddr_in& dst) noexcept {
    return ::sendto(fd_, buf.data(), buf.size(), 0,
                    reinterpret_cast<const sockaddr*>(&dst), sizeof(dst));
}

} 
