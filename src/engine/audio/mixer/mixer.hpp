#pragma once

#include "engine/audio/device/device.hpp"

namespace forge::audio {

class Mixer {
public:
    void set_master(float volume) { master_ = volume < 0 ? 0 : volume > 1 ? 1 : volume; }
    float master() const { return master_; }
    bool ready() const { return device().ready(); }

private:
    float master_ = 1.0f;
};

inline Mixer& mixer()
{
    static Mixer instance;
    return instance;
}

} // namespace forge::audio
