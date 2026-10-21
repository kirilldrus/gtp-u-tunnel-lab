#include "cn_socket.h"
#include "socket_helpers.h"
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

namespace upf {

void CnSocket::set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL failed");
}

CnSocket::CnSocket(const std::string& ifname) {
    fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd_ < 0) throw std::runtime_error("Failed to create RAW socket");

    const unsigned int ifindex = if_nametoindex(ifname.c_str());
    if (ifindex == 0) {
        ::close(fd_);
        throw std::runtime_error("Failed to get ifindex for " + ifname);
    }

    auto addr = detail::make_raw_socket_addr(static_cast<int>(ifindex));
    if (detail::bind_socket(fd_, addr) < 0) {
        ::close(fd_);
        throw std::runtime_error("Failed to bind RAW socket");
    }

    auto ifr = detail::to_ifreq(ifname);
    if (ioctl(fd_, SIOCGIFHWADDR, &ifr) < 0) {
        ::close(fd_);
        throw std::runtime_error("ioctl SIOCGIFHWADDR failed");
    }

    mac_ = detail::to_mac(ifr.ifr_hwaddr.sa_data);

    set_nonblocking(fd_);
}

CnSocket::~CnSocket() {
    if (fd_ >= 0) ::close(fd_);
}

std::ptrdiff_t CnSocket::recv(std::span<std::byte> buf) noexcept {
    return ::recv(fd_, buf.data(), buf.size(), 0);
}

std::ptrdiff_t CnSocket::send(std::span<const std::byte> buf) noexcept {
    return ::send(fd_, buf.data(), buf.size(), 0);
}

} 
