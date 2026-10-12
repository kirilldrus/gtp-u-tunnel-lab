#pragma once
#include "cn_socket.h"
#include "an_socket.h"
#include <array>
#include <cstdint>
#include <netinet/in.h>

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

    void handle_cn_ip(const uint8_t* frame, int len);
    void handle_cn_arp(const uint8_t* frame, int len);
    void handle_an_gtp(const uint8_t* datagram, int len);

    CnSocket cn_;
    AnSocket an_;

    sockaddr_in gnb_addr_{};

    std::array<uint8_t, 6> dn_mac_{};
    bool dn_mac_known_ = false;

    int epoll_fd_ = -1;
};

} 
