#include "upf_core.h"
#include "gtp.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <cstring>

namespace upf {

namespace {

constexpr std::uint16_t GTP_U_PORT = 2152;
constexpr const char*   UPF_AN_IP = "192.168.9.1";
constexpr const char*   GNB_IP = "192.168.9.2";
constexpr const char*   CN_IFNAME = "upf_cn_veth";

constexpr std::size_t ETH_HDR_LEN = 14;
constexpr std::size_t IP_HDR_MIN = 20;
constexpr std::size_t IP_SRC_OFF = 12;
constexpr std::size_t IP_DST_OFF = 16;
constexpr std::size_t IP_TOTLEN_OFF = 2;

constexpr std::uint16_t ETH_P_IP_N  = 0x0800;
constexpr std::uint16_t ETH_P_ARP_N = 0x0806;

constexpr std::size_t ARP_OP_OFF  = 6;
constexpr std::size_t ARP_SHA_OFF = 8;
constexpr std::size_t ARP_SPA_OFF = 14;
constexpr std::size_t ARP_THA_OFF = 18;
constexpr std::size_t ARP_TPA_OFF = 24;
constexpr std::size_t ARP_PAYLOAD = 28;

constexpr std::uint16_t ARPOP_REQUEST_N = 1;

constexpr std::uint8_t IP_OCTET1 = 192;
constexpr std::uint8_t IP_OCTET2 = 168;
constexpr std::uint8_t IP_OCTET3 = 7;
constexpr std::uint8_t IP_LAST_MIN = 1;
constexpr std::uint8_t IP_LAST_MAX = 4;

constexpr std::uint8_t IP_VER_4 = 4;
constexpr std::uint8_t IP_IHL_MIN = 5;
constexpr std::uint8_t IP_VERSION_SHIFT = 4;
constexpr std::uint8_t IP_IHL_MASK = 0x0F;
constexpr std::uint8_t IP_IHL_MULT = 4;

bool is_ue_ip(std::span<const std::uint8_t, 4> ip) noexcept {
    return ip[0] == IP_OCTET1 && ip[1] == IP_OCTET2 && ip[2] == IP_OCTET3 &&
           ip[3] >= IP_LAST_MIN && ip[3] <= IP_LAST_MAX;
}

struct UeEntry {
    std::uint8_t  last_octet;
    std::uint32_t teid_dl;
    std::uint32_t teid_ul;
};

constexpr std::array<UeEntry, 4> UE_TABLE = {{
    {1, 2, 3},
    {2, 4, 5},
    {3, 6, 7},
    {4, 8, 9},
}};

const UeEntry* find_ue_by_ip(std::uint8_t last_octet) noexcept {
    for (const auto& ue : UE_TABLE)
        if (ue.last_octet == last_octet) return &ue;
    return nullptr;
}

std::uint16_t be16(std::span<const std::byte> s) noexcept {
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(s[0]) << 8) |
         std::to_integer<std::uint16_t>(s[1]));
}

std::array<std::uint8_t, 4>
copy_ip4(std::span<const std::byte> src) noexcept {
    std::array<std::uint8_t, 4> out{};
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = std::to_integer<std::uint8_t>(src[i]);
    return out;
}

std::array<std::uint8_t, 6>
copy_mac6(std::span<const std::byte> src) noexcept {
    std::array<std::uint8_t, 6> out{};
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = std::to_integer<std::uint8_t>(src[i]);
    return out;
}

template <std::size_t N>
void copy_u8_to_bytes(std::span<std::byte> dst,
                      const std::array<std::uint8_t, N>& src) noexcept {
    const std::size_t n = dst.size() < N ? dst.size() : N;
    std::transform(src.begin(), src.begin() + n, dst.begin(),
                   [](std::uint8_t b) noexcept { return std::byte{b}; });
}

} // namespace

UpfCore::UpfCore()
    : cn_(CN_IFNAME),
      an_(UPF_AN_IP, GTP_U_PORT)
{
    gnb_addr_.sin_family = AF_INET;
    gnb_addr_.sin_port   = htons(GTP_U_PORT);
    inet_pton(AF_INET, GNB_IP, &gnb_addr_.sin_addr);

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) throw std::runtime_error("Failed to create epoll fd");

    epoll_event ev_cn{};
    ev_cn.events  = EPOLLIN;
    ev_cn.data.fd = cn_.fd();
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, cn_.fd(), &ev_cn) < 0)
        throw std::runtime_error("epoll_ctl ADD cn failed");

    epoll_event ev_an{};
    ev_an.events  = EPOLLIN;
    ev_an.data.fd = an_.fd();
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, an_.fd(), &ev_an) < 0)
        throw std::runtime_error("epoll_ctl ADD an failed");

    std::cout << "UPF Initialized successfully." << std::endl;
}

UpfCore::~UpfCore() {
    if (epoll_fd_ >= 0) ::close(epoll_fd_);
    std::cout << "UPF Shutdown complete." << std::endl;
}

void UpfCore::run() {
    std::array<epoll_event, 10> events{};

    for (;;) {
        const int n = epoll_wait(epoll_fd_, events.data(),
                                 static_cast<int>(events.size()), -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait failed: " << strerror(errno) << '\n';
            return;
        }
        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            try {
                if (fd == cn_.fd())      cn_read();
                else if (fd == an_.fd()) an_read();
            } catch (const std::exception& e) {
                std::cerr << "Error in main loop: " << e.what() << '\n';
            }
        }
    }
}

void UpfCore::cn_read() {
    std::array<std::byte, 2048> buf{};
    const auto n = cn_.recv(buf);
    if (n <= 0) return;

    const std::span<std::byte> frame{buf.data(),
                                     static_cast<std::size_t>(n)};
    if (frame.size() < ETH_HDR_LEN) return;

    const auto ethertype = be16(frame.subspan(12, 2));
    std::expected<void, std::string_view> result = {};

    if (ethertype == ETH_P_IP_N) {
        result = handle_cn_ip(frame);
    } else if (ethertype == ETH_P_ARP_N) {
        result = handle_cn_arp(frame);
    }

    if (!result) {
        std::cerr << "Dropped CN packet: " << result.error() << '\n';
    }
}

std::expected<void, std::string_view>
UpfCore::handle_cn_ip(std::span<const std::byte> frame) {
    if (frame.size() < ETH_HDR_LEN + IP_HDR_MIN)
        return std::unexpected("IP: frame too short");

    const auto ip = frame.subspan(ETH_HDR_LEN);
    const auto version_ihl = std::to_integer<std::uint8_t>(ip[0]);
    const auto version = version_ihl >> IP_VERSION_SHIFT;
    const auto ihl = version_ihl & IP_IHL_MASK;
    
    if (version != IP_VER_4 || ihl < IP_IHL_MIN)
        return std::unexpected("IP: not IPv4 or invalid IHL");

    const std::size_t ihl_bytes = static_cast<std::size_t>(ihl) * IP_IHL_MULT;
    if (ihl_bytes > ip.size())
        return std::unexpected("IP: IHL out of bounds");

    const auto tot_len = be16(ip.subspan(IP_TOTLEN_OFF, 2));
    if (tot_len < ihl_bytes || tot_len > ip.size())
        return std::unexpected("IP: invalid total length");

    auto ip_packet = ip.first(tot_len);
    if (ip_packet.size() < IP_DST_OFF + 4)
        return std::unexpected("IP: packet too short for dst IP");

    const auto dst_ip = copy_ip4(ip_packet.subspan(IP_DST_OFF, 4));
    if (!is_ue_ip(dst_ip))
        return std::unexpected("IP: destination not a UE");

    const auto* ue = find_ue_by_ip(dst_ip[3]);
    if (ue == nullptr)
        return std::unexpected("IP: UE not found");

    gtp::Header hdr{};
    hdr.flags    = gtp::FLAGS_G_PDU;
    hdr.msg_type = gtp::MSG_TYPE_G_PDU;
    hdr.length   = static_cast<std::uint16_t>(tot_len);
    hdr.teid     = ue->teid_dl;

    const auto gtp_bytes = gtp::serialize(hdr);
    std::array<std::byte, 2048> out{};
    std::copy(gtp_bytes.begin(), gtp_bytes.end(), out.begin());
    std::copy(ip_packet.begin(), ip_packet.end(),
              out.begin() + gtp_bytes.size());

    const std::size_t total = gtp_bytes.size() + tot_len;
    an_.send_to(std::span<const std::byte>{out.data(), total}, gnb_addr_);

    return {};
}

std::expected<void, std::string_view>
UpfCore::handle_cn_arp(std::span<std::byte> frame) {
    constexpr std::size_t ARP_LEN = ETH_HDR_LEN + ARP_PAYLOAD;
    if (frame.size() < ARP_LEN)
        return std::unexpected("ARP: frame too short");

    auto arp = frame.subspan(ETH_HDR_LEN, ARP_PAYLOAD);
    const auto ar_op = be16(arp.subspan(ARP_OP_OFF, 2));
    if (ar_op != ARPOP_REQUEST_N)
        return std::unexpected("ARP: not a request");

    const auto target_ip = copy_ip4(arp.subspan(ARP_TPA_OFF, 4));
    if (!is_ue_ip(target_ip))
        return std::unexpected("ARP: target IP not a UE");

    const auto sender_mac = copy_mac6(arp.subspan(ARP_SHA_OFF, 6));
    const auto sender_ip  = copy_ip4(arp.subspan(ARP_SPA_OFF, 4));

    std::copy(sender_mac.begin(), sender_mac.end(), dn_mac_.begin());
    dn_mac_known_ = true;

    copy_u8_to_bytes(frame.subspan(0, 6), sender_mac);
    copy_u8_to_bytes(frame.subspan(6, 6), cn_.mac());
    frame[12] = std::byte{0x08};
    frame[13] = std::byte{0x06};

    arp[ARP_OP_OFF] = std::byte{0x00};
    arp[ARP_OP_OFF + 1] = std::byte{0x02};
    copy_u8_to_bytes(arp.subspan(ARP_SHA_OFF, 6), cn_.mac());
    copy_u8_to_bytes(arp.subspan(ARP_SPA_OFF, 4), target_ip);
    copy_u8_to_bytes(arp.subspan(ARP_THA_OFF, 6), sender_mac);
    copy_u8_to_bytes(arp.subspan(ARP_TPA_OFF, 4), sender_ip);

    cn_.send(std::span<const std::byte>{frame.data(), ARP_LEN});
    return {};
}

void UpfCore::an_read() {
    std::array<std::byte, 2048> buf{};
    auto [size, peer] = an_.recv(buf);
    if (size <= 0) return;

    const std::span<const std::byte> datagram{
        buf.data(), static_cast<std::size_t>(size)};
        
    if (auto res = handle_an_gtp(datagram); !res) {
        std::cerr << "Dropped GTP packet: " << res.error() << '\n';
    }
}

std::expected<void, std::string_view>
UpfCore::handle_an_gtp(std::span<const std::byte> datagram) {
    if (datagram.size() < gtp::HEADER_SIZE + IP_HDR_MIN)
        return std::unexpected("GTP: datagram too short");

    const auto hdr = gtp::parse(datagram.first(gtp::HEADER_SIZE));
    if (hdr.flags != gtp::FLAGS_G_PDU || hdr.msg_type != gtp::MSG_TYPE_G_PDU)
        return std::unexpected("GTP: not a G-PDU");

    const std::size_t actual_payload = datagram.size() - gtp::HEADER_SIZE;
    if (hdr.length != actual_payload)
        return std::unexpected("GTP: length mismatch");

    const auto inner_ip = datagram.subspan(gtp::HEADER_SIZE);
    if (inner_ip.size() < IP_HDR_MIN)
        return std::unexpected("GTP: inner IP too short");

    const auto version_ihl = std::to_integer<std::uint8_t>(inner_ip[0]);
    if ((version_ihl >> IP_VERSION_SHIFT) != IP_VER_4)
        return std::unexpected("GTP: inner not IPv4");

    const std::size_t ihl = static_cast<std::size_t>(version_ihl & IP_IHL_MASK) * IP_IHL_MULT;
    if (ihl < IP_HDR_MIN || ihl > inner_ip.size())
        return std::unexpected("GTP: invalid IHL");

    const auto tot_len = be16(inner_ip.subspan(IP_TOTLEN_OFF, 2));
    if (tot_len < ihl || tot_len > inner_ip.size())
        return std::unexpected("GTP: invalid inner tot_len");

    auto ip_packet = inner_ip.first(tot_len);
    if (ip_packet.size() < IP_SRC_OFF + 4)
        return std::unexpected("GTP: packet too short for src IP");

    const auto src_ip = copy_ip4(ip_packet.subspan(IP_SRC_OFF, 4));
    if (!is_ue_ip(src_ip))
        return std::unexpected("GTP: source not a UE");

    const auto* ue = find_ue_by_ip(src_ip[3]);
    if (ue == nullptr)
        return std::unexpected("GTP: UE not found");

    if (hdr.teid != ue->teid_ul)
        return std::unexpected("GTP: TEID mismatch (spoofing?)");

    std::array<std::byte, 2048> eth_frame{};
    if (dn_mac_known_) {
        copy_u8_to_bytes(std::span{eth_frame}.subspan(0, 6), dn_mac_);
    } else {
        std::fill(eth_frame.begin(), eth_frame.begin() + 6,
                  std::byte{0xFF});
    }
    copy_u8_to_bytes(std::span{eth_frame}.subspan(6, 6), cn_.mac());
    eth_frame[12] = std::byte{0x08};
    eth_frame[13] = std::byte{0x00};

    std::copy(ip_packet.begin(), ip_packet.end(),
              eth_frame.begin() + ETH_HDR_LEN);

    const std::size_t total = ETH_HDR_LEN + tot_len;
    cn_.send(std::span<const std::byte>{eth_frame.data(), total});
    
    return {};
}

}
