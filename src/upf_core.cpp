#include "upf_core.h"
#include "gtp.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h> 
#include <sys/epoll.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

namespace upf {

UpfCore::UpfCore()
    : cn_("upf_cn_veth"),
      an_("192.168.9.1", 2152)
{
    gnb_addr_.sin_family = AF_INET;
    gnb_addr_.sin_port   = htons(2152);
    inet_pton(AF_INET, "192.168.9.2", &gnb_addr_.sin_addr);

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) throw std::runtime_error("Failed to create epoll fd");

    epoll_event ev_cn{};
    ev_cn.events  = EPOLLIN;
    ev_cn.data.fd = cn_.fd();
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, cn_.fd(), &ev_cn);

    epoll_event ev_an{};
    ev_an.events  = EPOLLIN;
    ev_an.data.fd = an_.fd();
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, an_.fd(), &ev_an);

    std::cout << "UPF Initialized successfully." << std::endl;
}

UpfCore::~UpfCore() {
    if (epoll_fd_ >= 0) ::close(epoll_fd_);
    std::cout << "UPF Shutdown complete." << std::endl;
}

void UpfCore::run() {
    std::array<epoll_event, 10> events{};

    for (;;) {
        const int n = epoll_wait(epoll_fd_, events.data(), 10, -1);
        if (n == -1) {
            std::cerr << "epoll_wait error" << std::endl;
            break;
        }
        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            if (fd == cn_.fd()) cn_read();
            else if (fd == an_.fd()) an_read();
        }
    }
}

void UpfCore::cn_read() {
    std::array<uint8_t, 2048> buf{};
    const int bytes = cn_.recv(buf.data(), buf.size());
    if (bytes <= 0) return;

    const auto* eth = reinterpret_cast<const ethhdr*>(buf.data());

    if (!dn_mac_known_) {
        std::memcpy(dn_mac_.data(), eth->h_source, dn_mac_.size());
        dn_mac_known_ = true;
    }

    if (eth->h_proto == htons(ETH_P_IP)) {
        handle_cn_ip(buf.data(), bytes);
    } else if (eth->h_proto == htons(ETH_P_ARP)) {
        handle_cn_arp(buf.data(), bytes);
    }
}

void UpfCore::handle_cn_ip(const uint8_t* frame, int len) {
    const auto* eth = reinterpret_cast<const ethhdr*>(frame);
    const uint8_t* ip_packet = frame + sizeof(ethhdr);
    const int ip_len = len - static_cast<int>(sizeof(ethhdr));

    const uint8_t* dst_ip = ip_packet + 16;
    if (dst_ip[0] != 192 || dst_ip[1] != 168 ||
        dst_ip[2] != 7 ||
        dst_ip[3] < 1 || dst_ip[3] > 4) {
        return;
    }

    const uint32_t teid_dl = dst_ip[3] * 2;

    gtp::Header gtp_hdr{};
    gtp_hdr.flags    = 0x30;
    gtp_hdr.msg_type = 0xFF;
    gtp_hdr.length   = htons(static_cast<uint16_t>(ip_len));
    gtp_hdr.teid     = htonl(teid_dl);

    std::array<uint8_t, 2048> send_buf{};
    std::memcpy(send_buf.data(), &gtp_hdr, sizeof(gtp_hdr));
    std::memcpy(send_buf.data() + sizeof(gtp_hdr), ip_packet, ip_len);

    const size_t total = sizeof(gtp_hdr) + static_cast<size_t>(ip_len);
    an_.send_to(send_buf.data(), total, gnb_addr_);
    (void)eth;
}

void UpfCore::handle_cn_arp(const uint8_t* frame, int len) {
    if (len < static_cast<int>(sizeof(ethhdr) + sizeof(ether_arp))) return;

    std::array<uint8_t, 2048> buf{};
    std::memcpy(buf.data(), frame, static_cast<size_t>(len));

    auto* eth = reinterpret_cast<ethhdr*>(buf.data());
    auto* arp = reinterpret_cast<ether_arp*>(buf.data() + sizeof(ethhdr));

    if (ntohs(arp->ea_hdr.ar_op) != ARPOP_REQUEST) return;

    if (arp->arp_tpa[0] != 192 ||
        arp->arp_tpa[1] != 168 ||
        arp->arp_tpa[2] != 7) {
        return;
    }

    std::array<uint8_t, 6> sender_mac{};
    std::array<uint8_t, 4> sender_ip{};
    std::array<uint8_t, 4> target_ip{};

    std::memcpy(sender_mac.data(), arp->arp_sha, sender_mac.size());
    std::memcpy(sender_ip.data(),  arp->arp_spa, sender_ip.size());
    std::memcpy(target_ip.data(),  arp->arp_tpa, target_ip.size());

    std::memcpy(eth->h_dest,   sender_mac.data(), 6);
    std::memcpy(eth->h_source, cn_.mac().data(),  6);

    arp->ea_hdr.ar_op = htons(ARPOP_REPLY);
    std::memcpy(arp->arp_sha, cn_.mac().data(),  6);
    std::memcpy(arp->arp_spa, target_ip.data(),  4);
    std::memcpy(arp->arp_tha, sender_mac.data(), 6);
    std::memcpy(arp->arp_tpa, sender_ip.data(),  4);

    cn_.send(buf.data(), sizeof(ethhdr) + sizeof(ether_arp));
}

void UpfCore::an_read() {
    std::array<uint8_t, 2048> buf{};
    sockaddr_in peer{};
    const int bytes = an_.recv(buf.data(), buf.size(), peer);
    if (bytes <= 0) return;

    handle_an_gtp(buf.data(), bytes);
}

void UpfCore::handle_an_gtp(const uint8_t* datagram, int len) {
    if (len < static_cast<int>(sizeof(gtp::Header) + 20)) return;

    const auto* gtp_hdr = reinterpret_cast<const gtp::Header*>(datagram);
    if (gtp_hdr->flags != 0x30) return;
    if (gtp_hdr->msg_type != 0xFF) return;

    const uint32_t teid = ntohl(gtp_hdr->teid);
    const uint8_t* inner_ip = datagram + sizeof(gtp::Header);
    const int inner_len = len - static_cast<int>(sizeof(gtp::Header));

    if (((inner_ip[0] >> 4) & 0x0F) != 4) return;

    const uint8_t* src_ip = inner_ip + 12;

    const bool valid =
        src_ip[0] == 192 &&
        src_ip[1] == 168 &&
        src_ip[2] == 7 &&
        src_ip[3] >= 1 &&
        src_ip[3] <= 4 &&
        teid == static_cast<uint32_t>(src_ip[3] * 2 + 1);

    if (!valid) return;

    std::array<uint8_t, 2048> eth_frame{};
    auto* out_eth = reinterpret_cast<ethhdr*>(eth_frame.data());

    if (dn_mac_known_) {
        std::memcpy(out_eth->h_dest, dn_mac_.data(), 6);
    } else {
        std::memset(out_eth->h_dest, 0xFF, 6);
    }
    std::memcpy(out_eth->h_source, cn_.mac().data(), 6);
    out_eth->h_proto = htons(ETH_P_IP);

    std::memcpy(eth_frame.data() + sizeof(ethhdr), inner_ip, inner_len);
    cn_.send(eth_frame.data(), sizeof(ethhdr) + static_cast<size_t>(inner_len));
}

} 
