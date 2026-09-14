#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

namespace forge::core {

struct ProfileSample {
    const char* name = "";
    double milliseconds = 0;
};

class Profiler {
public:
    void begin_frame()
    {
        samples_.clear();
        frame_start_ = Clock::now();
    }

    void end_frame()
    {
        frame_ms_ = elapsed_ms(frame_start_);
    }

    void push(const char* name)
    {
        stack_.push_back({name, Clock::now()});
    }

    void pop()
    {
        if (stack_.empty()) return;
        const auto top = stack_.back();
        stack_.pop_back();
        samples_.push_back({top.name, elapsed_ms(top.start)});
    }

    double frame_ms() const { return frame_ms_; }
    const std::vector<ProfileSample>& samples() const { return samples_; }

private:
    using Clock = std::chrono::steady_clock;
    struct Open {
        const char* name = "";
        Clock::time_point start{};
    };

    static double elapsed_ms(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    Clock::time_point frame_start_{};
    double frame_ms_ = 0;
    std::vector<Open> stack_;
    std::vector<ProfileSample> samples_;
};

class ProfileScope {
public:
    ProfileScope(Profiler& profiler, const char* name) : profiler_(&profiler) { profiler_->push(name); }
    ~ProfileScope()
    {
        if (profiler_) profiler_->pop();
    }
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    Profiler* profiler_ = nullptr;
};

inline Profiler& frame_profiler()
{
    static Profiler profiler;
    return profiler;
}

} // namespace forge::core
