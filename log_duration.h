#pragma once

#include <chrono>
#include <iostream>
#include <string>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration {
public:
    // ������� ��� ���� std::chrono::steady_clock
    // � ������� using ��� ��������
    using Clock = std::chrono::steady_clock;

    /*LogDuration(const std::string& id)
        : id_(id) {
    }*/

    LogDuration(const std::string& id, std::ostream& out = std::cerr)
        : id_(id),
          out_(out) {
        using namespace std::literals;
        if (&out_ == &std::cout) {
            id_ = "Operation time"s;
        }
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        out_ << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    std::string id_;
    const Clock::time_point start_time_ = Clock::now();
    std::ostream& out_ = std::cerr;
};