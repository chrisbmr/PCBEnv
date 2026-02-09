
#ifndef GYM_PCB_UTIL_UTIL_H
#define GYM_PCB_UTIL_UTIL_H

namespace util
{

bool lexiconumeric_compare(const std::string&, const std::string&);

std::string loadFile(const std::string& filePath);

inline std::string loadFile(const std::string_view filePath) { return loadFile(std::string(filePath)); }

std::string tolower(const std::string&);

bool is1TrueYesOrPlus(const char *);

inline bool is1TrueYesOrPlus(const std::string &s) { return is1TrueYesOrPlus(s.c_str()); }

} // namespace util

#endif // GYM_PCB_UTIL_UTIL_H
