#pragma once
#include "cn_socket.h"
#include "an_socket.h"
#include <array>
#include <cstdint>
#include <expected>
#include <netinet/in.h>
#include <span>
#include <string_view>

namespace upf {

class UpfCore {
public:
    UpfCore();
    ~UpfCore();

    UpfCore(const UpfCore&) = delete;
    UpfCore& operator=(const UpfCore&) = delete;

    void run();

private:
    void cn_read();
    void an_read();

    std::expected<void, std::string_view> handle_cn_ip(std::span<const std::byte> frame);
    std::expected<void, std::string_view> handle_cn_arp(std::span<std::byte> frame);
    std::expected<void, std::string_view> handle_an_gtp(std::span<const std::byte> datagram);

    CnSocket cn_;
    AnSocket an_;

    sockaddr_in gnb_addr_{};

    std::array<std::uint8_t, 6> dn_mac_{};
    bool dn_mac_known_ = false;

    int epoll_fd_ = -1;
};

} 
