
#include "Defs.hpp"
#include <fstream>
#include <sstream>

namespace util
{

std::string loadFile(const std::string &filePath)
{
    std::ifstream fs(filePath);
    if (!fs.is_open())
        throw std::runtime_error(fmt::format("File not found: {}", filePath));
    std::stringstream ss;
    ss << fs.rdbuf();
    return ss.str();
}

std::string tolower(const std::string &_s)
{
    std::string s = _s;
    std::transform(s.begin(), s.end(), s.begin(), [](char c){ return std::tolower(c); });
    return s;
}

bool is1TrueYesOrPlus(const char *s)
{
    if (!s)
        return false;
    return s[0] == '1' || s[0] == 't' || s[0] == 'T' || s[0] == 'y' || s[0] == 'Y' || s[0] == '+';
}

bool lexiconumeric_compare(const std::string &s1, const std::string &s2)
{
    size_t i1 = 0;
    size_t i2 = 0;
    while (i1 < s1.size() && i2 < s2.size()) {
        if (std::isdigit(s1[i1]) && std::isdigit(s2[i2])) {
            const size_t n1 = i1;
            const size_t n2 = i2;
            for (++i1; i1 < s1.size() && std::isdigit(s1[i1]); ++i1);
            for (++i2; i2 < s2.size() && std::isdigit(s2[i2]); ++i2);
#ifdef GYM_PCB_NO_STDFORMAT
            uint64_t v1 = std::stol(std::string(&s1[n1], i1-n1));
            uint64_t v2 = std::stol(std::string(&s2[n2], i2-n2));
#else
            uint64_t v1, v2;
            std::from_chars(&s1[n1], &s1[i1], v1);
            std::from_chars(&s2[n2], &s2[i2], v2);
#endif
            if (v1 != v2)
                return v1 < v2;
        } else {
            if (s1[i1] != s2[i2])
                return s1[i1] < s2[i2];
            i1++;
            i2++;
        }
    }
    return s1.size() < s2.size();
}

} // namespace util
