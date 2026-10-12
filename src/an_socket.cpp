#include "an_socket.h"
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace upf {

void AnSocket::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

AnSocket::AnSocket(const std::string& bind_ip, uint16_t port) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) throw std::runtime_error("Failed to create UDP socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, bind_ip.c_str(), &addr.sin_addr) != 1) {
        ::close(fd_);
        throw std::runtime_error("inet_pton failed for " + bind_ip);
    }

    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd_);
        throw std::runtime_error("Failed to bind UDP socket");
    }

    set_nonblocking(fd_);
}

AnSocket::~AnSocket() {
    if (fd_ >= 0) ::close(fd_);
}

int AnSocket::recv(uint8_t* buf, size_t len, sockaddr_in& peer) noexcept {
    socklen_t peer_len = sizeof(peer);
    return static_cast<int>(::recvfrom(
        fd_, buf, len, 0,
        reinterpret_cast<sockaddr*>(&peer), &peer_len));
}

int AnSocket::send_to(const uint8_t* buf, size_t len,
                      const sockaddr_in& dst) noexcept {
    return static_cast<int>(::sendto(
        fd_, buf, len, 0,
        reinterpret_cast<const sockaddr*>(&dst), sizeof(dst)));
}

} 
