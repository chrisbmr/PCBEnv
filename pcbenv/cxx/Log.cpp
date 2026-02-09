
#include <filesystem>
#include <iostream>
#include "Log.hpp"
#include "Util/Util.hpp"

Logger Logger::sDefault;

const char Logger::sLevelChars[Logger::NumLevels] = { 'E', 'W', 'N', 'I', 'D', 'T' };

Logger::Logger()
{
    mFiles[uint(LogLevel::ERROR)].setPath("cerr");
    mFiles[uint(LogLevel::WARNING)].setPath("cerr");
    mFiles[uint(LogLevel::NOTICE)].setPath("cout");
    mFiles[uint(LogLevel::INFO)].setPath("cout");
    mFiles[uint(LogLevel::DEBUG)].setPath("cout");
    mFiles[uint(LogLevel::TRACE)].setPath("cout");
    setDefaultLevel("I");
    // std::cout << "Logger verbosity: " << uint(mLevel) << std::endl;
}

void Logger::setDefaultLevel(const std::string &name)
{
    const char *v = std::getenv("LOGV");
    if (v)
        setLevel(std::string(v));
    else
        setLevel(name);
}

Logger::~Logger()
{
}

LogLevel Logger::level(const std::string_view name)
{
    if (name.empty())
        return LogLevel::INFO;
    switch (std::tolower(name[0])) {
    case 'e': return LogLevel::ERROR;
    case 'w': return LogLevel::WARNING;
    case 'n': return LogLevel::NOTICE;
    case 'i': return LogLevel::INFO;
    case 'd': return LogLevel::DEBUG;
    case 't': return LogLevel::TRACE;
    default:
        break;
    }
    return LogLevel::INFO;
}

void LogFile::shut(const std::string_view path)
{
    mStream = (path == "cerr") ? &std::cerr : &std::cout;
    if (mOwner == this)
        mFile.close();
    mOwner = 0;
    mPath = (path == "cerr") ? path : "cout";
}
void LogFile::redirect(LogFile &target)
{
    assert(&target != this && target.mOwner != this);
    shut();
    mOwner = target.mOwner;;
    mStream = target.mStream;
    mPath = target.mPath;
}
void LogFile::open(const std::string_view path)
{
    assert(!mFile.is_open());
    truncate(path);
    mFile.open(std::string(path), std::ios::app);
    if (!mFile.is_open())
        return;
    mStream = &mFile;
    mPath = path;
    mOwner = this;
}
void LogFile::truncate(const std::string_view path)
{
    if (!std::filesystem::exists(path) || std::filesystem::file_size(path) < MaxSize)
        return;
    const auto s = util::loadFile(path);
    std::size_t start = MaxSize / 2;
    while (start < s.size() && s[start - 1] != '\n') ++start;
    std::ofstream ofs{std::string(path)};
    if (ofs.is_open())
        ofs << std::string_view(s.data() + start, s.size() - start);
}
void LogFile::setPath(const std::string_view path)
{
    if (mPath == path)
        return;
    shut(path);
    if (path != "cerr" && path != "cout")
        open(path);
}
void LogFile::ownFile()
{
    assert(mOwner);
    mFile = std::move(mOwner->mFile);
    mOwner = mOwner->mOwner = this;
}
void LogFile::updateOwner()
{
    if (mOwner)
        mOwner = mOwner->mOwner;
}

void Logger::setTarget(LogLevel l, const std::string &path)
{
    auto &F = mFiles[uint(l)];
    if (F.getPath() == path)
        return;
    if (F.isOwner()) {
        for (auto &I : mFiles) {
            if (&I != &F && I.owner() == &F) {
                I.ownFile();
                break;
            }
        }
        if (!F.isOwner())
            for (auto &I : mFiles)
                I.updateOwner();
    }
    for (auto &I : mFiles) {
        if (I.getPath() == path) {
            F.redirect(I);
            break;
        }
    }
    F.setPath(path);
}

std::string Logger::formatDurationUS(uint64_t _us)
{
    double us = _us;
    constexpr const uint64_t msec = 1000;
    constexpr const uint64_t sec = msec * 1000;
    constexpr const uint64_t min = sec * 60;
    constexpr const uint64_t hour = min * 60;
    constexpr const uint64_t day = hour * 24;
    if (us == 0)
        return "0";
    if (us < msec)
        return std::to_string(_us) + " µs";
    if (us < sec)
        return fmt::format("{:.1f} ms", us * (1.0 / msec));
    if (us < min)
        return fmt::format("{:.1f} s", us * (1.0 / sec));
    if (us < hour)
        return fmt::format("{:.1f} m", us * (1.0 / min));
    if (us < day)
        return fmt::format("{:.1f} h", us * (1.0 / hour));
    return fmt::format("{:.1f} d", us * (1.0 / day));
}

std::string Logger::formatMask(uint32_t mask)
{
    if (mask == 0xffffffff)
        return "*";
    std::stringstream ss;
    ss << '{';
    while (mask) {
        const uint i = std::countr_zero(mask);
        const uint m = 1 << i;
        ss << i;
        if (mask != m)
            ss << '|';
        mask &= ~m;
    }
    ss << '}';
    return ss.str();
}

std::string Logger::nowString()
{
    const auto t = std::chrono::system_clock::now();
#ifndef GYM_PCB_NO_STDFORMAT
    if (GYM_PCB_LOG_FORMAT_TIME)
        return fmt::format("{:%F %T}", std::chrono::time_point_cast<std::chrono::microseconds>(t));
#endif
    return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count());
}
