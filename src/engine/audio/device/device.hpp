#pragma once

namespace forge::audio {

// Reserved mixer/voice/bank/spatialization live here later (miniaudio or SDL
// audio). Game code must emit events into this module, not call SDL audio
// directly. Device currently claims the SDL audio subsystem.
class Device {
public:
    bool open();
    void close();
    bool ready() const { return ready_; }

private:
    bool ready_ = false;
};

Device& device();

} // namespace forge::audio
