#pragma once

#include <chrono>
#include <ctime>
#include <iostream>

namespace cc {

enum class LogLevel {
    Warning,
    Debug
};

static inline LogLevel s_current_log_level = LogLevel::Debug;

template<typename ... Args>
void log(LogLevel level, Args && ... args)
{
    if (level > s_current_log_level) {
        return;
    }

    std::time_t now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cerr << std::ctime(&now_t) << ": ";
    ((std::cerr << args), ...);
    std::cerr << std::endl;
}

} // namespace cc