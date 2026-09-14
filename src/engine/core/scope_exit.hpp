#pragma once

#include <utility>

namespace forge {

template<class F>
class ScopeExit {
public:
    explicit ScopeExit(F cleanup) : cleanup_(std::move(cleanup)) {}
    ~ScopeExit() { cleanup_(); }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    F cleanup_;
};

} // namespace forge
