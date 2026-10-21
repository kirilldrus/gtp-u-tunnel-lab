#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <string>
#include <stdexcept>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

namespace upf::detail {

using mac_address_t = std::array<std::uint8_t, 6>;

inline sockaddr_in make_udp_addr(const std::string& ip, std::uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("inet_pton failed for " + ip);
    }
    return addr;
}

inline ifreq to_ifreq(const std::string& name) {
    ifreq ifr{};
    const std::size_t len = std::min(name.size(), sizeof(ifr.ifr_name) - 1);
    std::copy_n(name.begin(), len, ifr.ifr_name);
    ifr.ifr_name[len] = '\0';
    return ifr;
}

inline mac_address_t to_mac(const char* sa_data) {
    mac_address_t mac{};
    for (std::size_t i = 0; i < mac.size(); ++i) {
        mac[i] = static_cast<std::uint8_t>(sa_data[i]);
    }
    return mac;
}

inline sockaddr_ll make_raw_socket_addr(int ifindex) {
    sockaddr_ll addr{};
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex  = ifindex;
    return addr;
}

inline int bind_socket(int fd, const sockaddr_in& addr) {
    return ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

inline int bind_socket(int fd, const sockaddr_ll& addr) {
    return ::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

} 
