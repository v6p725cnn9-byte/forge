#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace forge::net {

struct Address {
    std::uint32_t host = 0;
    std::uint16_t port = 0;
    bool operator==(const Address&) const = default;
};

Address localhost(std::uint16_t port);
Address any(std::uint16_t port);
bool parse_address(const std::string& text, Address& out);
std::vector<std::string> ipv4_addresses();

class Udp {
public:
    Udp() = default;
    ~Udp() { close(); }
    Udp(const Udp&) = delete;
    Udp& operator=(const Udp&) = delete;

    bool open();
    bool bind(const Address& address);
    void close();
    bool send(const Address& to, const void* data, std::size_t size);
    int receive(Address& from, void* data, std::size_t size);
    std::uint16_t port() const { return bound_.port; }
    bool valid() const;

private:
#ifdef _WIN32
    std::uintptr_t fd_ = static_cast<std::uintptr_t>(-1);
#else
    int fd_ = -1;
#endif
    Address bound_{};
};

} // namespace forge::net
