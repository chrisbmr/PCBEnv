
#ifndef GYM_PCB_LOADERS_DSNPARSERBASE_H
#define GYM_PCB_LOADERS_DSNPARSERBASE_H

#include "Loaders/DSN.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
#include <regex>

class Token
{
public:
    Token(const std::string &s) : _s(s) { }
    size_t length() const { return _s.size(); }
    const std::string& String() const { return _s; }
    int64_t Int64() const { return std::stol(_s); }
    int AngleDeg() const;
    double Float64() const;
    bool Bool() const;
private:
    const std::string _s;
};
inline double Token::Float64() const
{
    double d;
    std::from_chars(_s.data(), _s.data() + _s.size(), d);
    return d;
}

class DSNParserBase
{
public:
    DSNParserBase(dsn::Data&);
    virtual bool parse(const std::string&) = 0;
protected:
    dsn::Data &mData;
    char mStringQuote{0};
    char mEscapeChar{0};
    bool mQuoteOpen{false};
    bool mEscapeActive{false};
protected:
    bool checkQuote(char c);
    void resetQuote();
    int isBracket(char c) const;
    int checkBracket(char c);
    size_t nextBracket(const std::string&, int dir);
    size_t skipspace(const std::string&, size_t pos) const;
    size_t skipspaceRev(const std::string&, size_t minPos, size_t pos) const;
    std::vector<std::string> gather(const std::string&, const char *name, const int depth = 1);
    std::vector<std::string> gatherArray(const std::string&);
    std::vector<Token> tokenize(const std::string&);
    Token firstToken(const std::string&, size_t *from = 0);
    std::string unquote(const std::string&) const;
    std::string unquotePin(const std::string&);
    std::string noHyphen(std::string) const;

    int unitToExp10(const std::string&) const;
    char parseSide(const std::string&) const;
};

inline std::string DSNParserBase::noHyphen(std::string s) const
{
    for (auto &c : s)
        if (c == '-')
            c = '_';
    return s;
}

#endif // GYM_PCB_LOADERS_DSNPARSERBASE_H
