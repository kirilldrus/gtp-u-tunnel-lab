#include "cn_socket.h"
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

namespace upf {

void CnSocket::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

CnSocket::CnSocket(const std::string& ifname) {
    fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd_ < 0) throw std::runtime_error("Failed to create RAW socket");

    const unsigned int ifindex = if_nametoindex(ifname.c_str());
    if (ifindex == 0) {
        ::close(fd_);
        throw std::runtime_error("Failed to get ifindex for " + ifname);
    }

    sockaddr_ll addr{};
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex  = static_cast<int>(ifindex);

    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd_);
        throw std::runtime_error("Failed to bind RAW socket");
    }

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    if (ioctl(fd_, SIOCGIFHWADDR, &ifr) < 0) {
        ::close(fd_);
        throw std::runtime_error("ioctl SIOCGIFHWADDR failed");
    }
    std::memcpy(mac_.data(), ifr.ifr_hwaddr.sa_data, mac_.size());

    set_nonblocking(fd_);
}

CnSocket::~CnSocket() {
    if (fd_ >= 0) ::close(fd_);
}

int CnSocket::recv(uint8_t* buf, size_t len) noexcept {
    return static_cast<int>(::recv(fd_, buf, len, 0));
}

int CnSocket::send(const uint8_t* buf, size_t len) noexcept {
    return static_cast<int>(::send(fd_, buf, len, 0));
}

}
