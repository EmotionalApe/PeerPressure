#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <ctime>

struct ProtocolEvent {
    std::string timestamp; // "12:04:11"
    std::string severity;  // "INFO", "WARNING", "ERROR"
    std::string message;
};

class EventLogger {
private:
    std::vector<ProtocolEvent> events;
    mutable std::mutex mutex;
    const size_t max_events = 200;

    EventLogger() = default;

public:
    static EventLogger& instance() {
        static EventLogger inst;
        return inst;
    }

    void log(const std::string& message, const std::string& severity = "INFO") {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        struct tm tm_now;
#ifdef _WIN32
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif
        char buf[16];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_now);

        events.push_back({buf, severity, message});
        if (events.size() > max_events) {
            events.erase(events.begin());
        }
    }

    std::vector<ProtocolEvent> get_events() const {
        std::lock_guard<std::mutex> lock(mutex);
        return events;
    }
};
