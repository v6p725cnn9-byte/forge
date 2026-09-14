#include "engine/net/socket.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <charconv>
#include <cstring>

namespace forge::net {
namespace {

#ifdef _WIN32
using Handle = SOCKET;
using socklen_t = int;
constexpr Handle kInvalid = INVALID_SOCKET;
void startup()
{
    static bool ready = false;
    if (ready) return;
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    ready = true;
}
bool set_nonblock(Handle fd)
{
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
}
int last_would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
void close_handle(Handle fd) { closesocket(fd); }
#else
using Handle = int;
constexpr Handle kInvalid = -1;
void startup() {}
bool set_nonblock(Handle fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
int last_would_block() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNREFUSED; }
void close_handle(Handle fd) { ::close(fd); }
#endif

sockaddr_in to_sock(const Address& address)
{
    sockaddr_in out{};
    out.sin_family = AF_INET;
    out.sin_port = htons(address.port);
    out.sin_addr.s_addr = htonl(address.host);
    return out;
}

Address from_sock(const sockaddr_in& in)
{
    return {ntohl(in.sin_addr.s_addr), ntohs(in.sin_port)};
}

Handle as_handle(std::uintptr_t fd) { return static_cast<Handle>(fd); }

} // namespace

Address localhost(std::uint16_t port) { return {0x7F000001u, port}; }
Address any(std::uint16_t port) { return {0u, port}; }

bool parse_address(const std::string& text, Address& out)
{
    const auto colon = text.rfind(':');
    const std::string host = colon == std::string::npos ? text : text.substr(0, colon);
    std::uint16_t port = 27015;
    if (colon != std::string::npos) {
        const auto digits = text.substr(colon + 1);
        unsigned value = 0;
        const auto [end, err] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (err != std::errc{} || end != digits.data() + digits.size() || value == 0 || value > 65535u) return false;
        port = static_cast<std::uint16_t>(value);
    }
    in_addr addr{};
    if (inet_pton(AF_INET, host.c_str(), &addr) != 1) return false;
    out.host = ntohl(addr.s_addr);
    out.port = port;
    return true;
}

bool Udp::valid() const { return as_handle(fd_) != kInvalid; }

bool Udp::open()
{
    startup();
    close();
    const Handle fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == kInvalid) return false;
    if (!set_nonblock(fd)) {
        close_handle(fd);
        return false;
    }
#ifdef _WIN32
    fd_ = static_cast<std::uintptr_t>(fd);
#else
    fd_ = fd;
#endif
    return true;
}

bool Udp::bind(const Address& address)
{
    if (!valid() && !open()) return false;
    const auto sock = to_sock(address);
    if (::bind(as_handle(fd_), reinterpret_cast<const sockaddr*>(&sock), sizeof(sock)) != 0) {
        close();
        return false;
    }
    sockaddr_in actual{};
    socklen_t len = sizeof(actual);
    if (getsockname(as_handle(fd_), reinterpret_cast<sockaddr*>(&actual), &len) == 0) bound_ = from_sock(actual);
    else bound_ = address;
    return true;
}

void Udp::close()
{
    if (!valid()) return;
    close_handle(as_handle(fd_));
#ifdef _WIN32
    fd_ = static_cast<std::uintptr_t>(-1);
#else
    fd_ = -1;
#endif
    bound_ = {};
}

bool Udp::send(const Address& to, const void* data, std::size_t size)
{
    if (!valid()) return false;
    const auto sock = to_sock(to);
    const int sent = static_cast<int>(
        sendto(as_handle(fd_), static_cast<const char*>(data), static_cast<int>(size), 0,
               reinterpret_cast<const sockaddr*>(&sock), sizeof(sock)));
    return sent == static_cast<int>(size);
}

int Udp::receive(Address& from, void* data, std::size_t size)
{
    if (!valid()) return 0;
    sockaddr_in sock{};
    socklen_t len = sizeof(sock);
    const int got = static_cast<int>(
        recvfrom(as_handle(fd_), static_cast<char*>(data), static_cast<int>(size), 0,
                 reinterpret_cast<sockaddr*>(&sock), &len));
    if (got < 0) return last_would_block() ? 0 : -1;
    from = from_sock(sock);
    return got;
}

std::vector<std::string> ipv4_addresses()
{
    std::vector<std::string> out;
#ifndef _WIN32
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return out;
    for (auto* iface = list; iface; iface = iface->ifa_next) {
        if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET) continue;
        const auto* in = reinterpret_cast<sockaddr_in*>(iface->ifa_addr);
        const auto host = ntohl(in->sin_addr.s_addr);
        if ((host & 0xFF000000u) == 0x7F000000u) continue;
        char buf[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf));
        if (buf[0]) out.emplace_back(buf);
    }
    freeifaddrs(list);
#endif
    return out;
}

} // namespace forge::net
