#pragma once
#include <cstdint>
#include <string>
#include <netinet/in.h>

namespace upf {

class AnSocket {
public:
    AnSocket(const std::string& bind_ip, uint16_t port);
    ~AnSocket();

    AnSocket(const AnSocket&) = delete;
    AnSocket& operator=(const AnSocket&) = delete;

    int fd() const noexcept { return fd_; }

    int recv(uint8_t* buf, size_t len, sockaddr_in& peer) noexcept;
    int send_to(const uint8_t* buf, size_t len, const sockaddr_in& dst) noexcept;

private:
    static void set_nonblocking(int fd);

    int fd_ = -1;
};

}
