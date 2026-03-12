#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * Thread-safe Singleton Logger class
 * 
 * Provides centralized logging functionality with timestamps and log levels.
 * All log entries are stored in a vector and can be retrieved for display.
 */
class Logger {
public:
    /**
     * Get the singleton instance
     * @return Reference to the Logger singleton
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    // Delete copy constructor and assignment operator (Singleton pattern)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    /**
     * Add a log entry with level and message
     * @param level Log level (e.g., "INFO", "WARNING", "ERROR")
     * @param message The log message
     */
    void addLog(const std::string& level, const std::string& message);
    
    /**
     * Get all log entries (reference; may be invalidated by addLog from another path).
     * Prefer getLogsCopy() when iterating over logs while other code may call addLog() (e.g. during load).
     */
    const std::vector<std::string>& getLogs() const { return m_logs; }
    
    /**
     * Get a copy of all log entries under lock. Safe to iterate while addLog may be called.
     * Use in UI (e.g. Event Log) to avoid use-after-free if logs are added during rendering.
     */
    std::vector<std::string> getLogsCopy() const;
    
    /**
     * Clear all log entries
     */
    void clearLogs();
    
    /**
     * Clear all log entries (convenience method)
     */
    void clear() { clearLogs(); }
    
    /**
     * Get the maximum number of log entries to keep
     * @return Maximum log entries
     */
    size_t getMaxLogEntries() const { return MAX_LOG_ENTRIES; }
    
private:
    // Private constructor (Singleton pattern)
    Logger() = default;
    ~Logger() = default;
    
    // Log storage
    mutable std::mutex m_mutex;  // Mutex for thread safety
    std::vector<std::string> m_logs;
    
    // Maximum number of log entries to keep (prevents memory growth)
    static const size_t MAX_LOG_ENTRIES = 1000;
    
    /**
     * Generate a timestamp string
     * @return Formatted timestamp string
     */
    std::string getTimestamp() const;
};

// Convenience macros for logging
#define LOG_INFO(msg) Logger::getInstance().addLog("INFO", msg)
#define LOG_WARNING(msg) Logger::getInstance().addLog("WARNING", msg)
#define LOG_ERROR(msg) Logger::getInstance().addLog("ERROR", msg)
#define LOG_DEBUG(msg) Logger::getInstance().addLog("DEBUG", msg)

#endif // LOGGER_H
