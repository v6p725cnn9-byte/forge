#pragma once

#include "engine/net/transport/channel.hpp"

namespace forge::net {

inline constexpr std::uint32_t transport_magic() { return kTransportMagic; }
inline constexpr std::size_t envelope_bytes() { return kEnvelopeBytes; }

} // namespace forge::net
