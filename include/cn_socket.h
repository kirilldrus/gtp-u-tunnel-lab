#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace upf {

class CnSocket {
public:
    explicit CnSocket(const std::string& ifname);
    ~CnSocket();

    CnSocket(const CnSocket&) = delete;
    CnSocket& operator=(const CnSocket&) = delete;

    int fd() const noexcept { return fd_; }

    const std::array<uint8_t, 6>& mac() const noexcept { return mac_; }

    int recv(uint8_t* buf, size_t len) noexcept;
    int send(const uint8_t* buf, size_t len) noexcept;

private:
    static void set_nonblocking(int fd);

    int fd_ = -1;
    std::array<uint8_t, 6> mac_{};
};

} 
