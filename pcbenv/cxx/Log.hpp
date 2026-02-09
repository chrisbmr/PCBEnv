
#ifndef GYM_PCB_LOG_H
#define GYM_PCB_LOG_H

#include "Defs.hpp"
#include <iomanip>
#include <chrono>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <thread>

enum class LogLevel
{
    ERROR = 0,
    WARNING = 1,
    NOTICE = 2,
    INFO = 3,
    DEBUG = 4,
    TRACE = 5,
    COUNT = 6
};

#ifndef GYM_PCB_MAX_LOG_LEVEL
#define GYM_PCB_MAX_LOG_LEVEL 3
#endif
#ifndef GYM_PCB_LOG_SHOW_THREAD
#define GYM_PCB_LOG_SHOW_THREAD 0
#endif
#ifndef GYM_PCB_LOG_FORMAT_TIME
#define GYM_PCB_LOG_FORMAT_TIME 1
#endif

#define _PCB_GYM_LOG_IF(v, func, ...) if (int(v) <= GYM_PCB_MAX_LOG_LEVEL && Logger::get().visible(v)) { std::stringstream ___ss; ___ss << __VA_ARGS__; Logger::get().func(___ss.str()); }

#define ERROR(...)  do { _PCB_GYM_LOG_IF(LogLevel::ERROR, Error, __VA_ARGS__) } while (0)
#define WARN(...)   do { _PCB_GYM_LOG_IF(LogLevel::WARNING, Warn, __VA_ARGS__) } while (0)
#define INFO(...)   do { _PCB_GYM_LOG_IF(LogLevel::INFO, Info, __VA_ARGS__) } while (0)
#define NOTICE(...) do { _PCB_GYM_LOG_IF(LogLevel::NOTICE, Notice, __VA_ARGS__) } while (0)
#define DEBUG(...)  do { _PCB_GYM_LOG_IF(LogLevel::DEBUG, Debug, __VA_ARGS__) } while (0)
#define TRACE(...)  do { _PCB_GYM_LOG_IF(LogLevel::TRACE, Trace, __VA_ARGS__) } while (0)

#define RETURN_ERROR(rv, ...) do { ERROR(__VA_ARGS__); return rv; } while (0)
#define RETURN_INFO(rv, ...) do { INFO(__VA_ARGS__); return rv; } while (0)

class LogFile
{
public:
    constexpr static const std::size_t MaxSize = 32ull << 20;
public:
    std::ostream& stream() { return *mStream; }
    const std::string& getPath() const { return mPath; }
    void setPath(const std::string_view);
    void redirect(LogFile&);
    bool isOwner() const { return mOwner == this; }
    LogFile *owner() const { return mOwner; }
    void ownFile();
    void updateOwner();
private:
    std::ostream *mStream{0};
    std::string mPath;
    std::ofstream mFile;
    LogFile *mOwner{0};
private:
    void open(const std::string_view path);
    void shut(const std::string_view path = "cerr");
    void truncate(const std::string_view path);
};

class Logger
{
public:
    constexpr static const uint NumLevels = uint(LogLevel::COUNT);
    Logger();
    ~Logger();

    static Logger& get() { return sDefault; }

    void Error (const std::string &s) { emit(LogLevel::ERROR, " EE ", s); }
    void Warn  (const std::string &s) { emit(LogLevel::WARNING, " WW ", s); }
    void Info  (const std::string &s) { emit(LogLevel::INFO, " II ", s); }
    void Notice(const std::string &s) { emit(LogLevel::NOTICE, " NN ", s); }
    void Debug (const std::string &s) { emit(LogLevel::DEBUG, " DD ", s); }
    void Trace (const std::string &s) { emit(LogLevel::TRACE, " TT ", s); }

    bool visible(LogLevel v) const { return mLevel >= v; }

    void setLevel(const std::string&);
    void setDefaultLevel(const std::string&);
    void setLevel(LogLevel);
    void setPrefix(const std::string&);
    void setTarget(LogLevel, const std::string &path);

    int indent(int n);

    static char levelChar(LogLevel);
    static LogLevel level(const std::string_view name);
    static std::string nowString();
    static std::string formatDurationUS(uint64_t us);
    static std::string formatMask(uint32_t);

private:
    static const char sLevelChars[NumLevels];
    static Logger sDefault;

private:
    LogLevel mLevel{LogLevel::INFO};
    std::string mPrefix;
    LogFile mFiles[NumLevels];
    int mIndent{0};

    void emit(LogLevel, const char *pfx, const std::string&);
};

inline char Logger::levelChar(LogLevel l)
{
    assert(uint(l) < NumLevels);
    return sLevelChars[uint(l)];
}

inline int Logger::indent(int n)
{
    mIndent = std::max(0, mIndent + n);
    mPrefix = std::string(mIndent, ' ');
    return mIndent;
}

inline void Logger::setLevel(const std::string &name)
{
    mLevel = level(name);
}
inline void Logger::setLevel(LogLevel v)
{
    mLevel = v;
}

inline void Logger::setPrefix(const std::string &s)
{
    mPrefix = s;
    indent(0);
}

inline void Logger::emit(LogLevel l, const char *pfx, const std::string &s)
{
    auto &os = mFiles[uint(l)].stream();
    if (GYM_PCB_LOG_SHOW_THREAD)
        os << '<' << std::this_thread::get_id() << "> ";
    os << nowString() << pfx << mPrefix << s << std::endl;
}

#endif // GYM_PCB_LOG_H
