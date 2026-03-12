#include "logger.h"
#include <ctime>

void Logger::addLog(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Generate timestamp
    std::string timestamp = getTimestamp();
    
    // Format: [TIMESTAMP] [LEVEL] message
    std::string logEntry = "[" + timestamp + "] [" + level + "] " + message;
    
    // Add to logs
    m_logs.push_back(logEntry);
    
    // Limit log size to prevent memory growth
    if (m_logs.size() > MAX_LOG_ENTRIES) {
        // Remove oldest entries (keep the most recent MAX_LOG_ENTRIES)
        m_logs.erase(m_logs.begin(), m_logs.begin() + (m_logs.size() - MAX_LOG_ENTRIES));
    }
}

void Logger::clearLogs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
}

std::vector<std::string> Logger::getLogsCopy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_logs;
}

std::string Logger::getTimestamp() const {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // Convert to local time
    struct tm timeinfo;
    #ifdef _WIN32
        localtime_s(&timeinfo, &time_t);
    #else
        localtime_r(&time_t, &timeinfo);
    #endif
    
    // Format: HH:MM:SS
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << timeinfo.tm_hour << ":"
        << std::setfill('0') << std::setw(2) << timeinfo.tm_min << ":"
        << std::setfill('0') << std::setw(2) << timeinfo.tm_sec;
    
    return oss.str();
}
