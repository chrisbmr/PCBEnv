
#include "Log.hpp"
#include "Loaders/DSNParserBase.hpp"

int Token::AngleDeg() const
{
    auto a = Int64();
    if (a < 0)
        a += 360;
    if (a < 0 || a >= 360)
        throw std::runtime_error(fmt::format("Expected angle between 0 and 359°, got: {}", _s));
    return a;
}
bool Token::Bool() const
{
    if (_s.empty() || _s[0] == '0')
        return false;
    const char c = std::tolower(_s[0]);
    if (c == 'n')
        return false;
    if (c == 'o')
        return _s.size() > 1 && std::tolower(_s[1]) == 'n';
    return true;
}

int DSNParserBase::unitToExp10(const std::string &s) const
{
    if (s == "nm") return -9;
    if (s == "mm") return -3;
    assert(s == "um");
    return -6;
}
char DSNParserBase::parseSide(const std::string &s) const
{
    if (s == "Top" || s == "F.Cu") return 't';
    if (s == "Bottom" || s == "B.Cu") return 'b';
    return 'i';
}

inline size_t DSNParserBase::skipspace(const std::string &s, size_t pos) const
{
    while (pos < s.size() && std::isspace(s[pos]))
        ++pos;
    return pos;
}
inline size_t DSNParserBase::skipspaceRev(const std::string &s, size_t minPos, size_t pos) const
{
    while (pos > minPos && std::isspace(s[pos]))
        --pos;
    return pos;
}

inline void DSNParserBase::resetQuote()
{
    mQuoteOpen = mEscapeActive = false;
}

size_t DSNParserBase::nextBracket(const std::string &s, int dir)
{
    size_t i = 0;
    for (i = 0; i < s.size(); ++i)
        if (checkBracket(s[i]) == dir)
            break;
    assert(i == s.size() || (!mQuoteOpen && !mEscapeActive));
    if (i < s.size())
        return i;
    resetQuote();
    return std::string::npos;
}

inline bool DSNParserBase::checkQuote(char c)
{
    if (c == mStringQuote && !mEscapeActive)
        mQuoteOpen = !mQuoteOpen;
    mEscapeActive = (c == mEscapeChar);
    return mQuoteOpen;
}
inline int DSNParserBase::checkBracket(char c)
{
    return checkQuote(c) ? 0 : isBracket(c);
}
inline int DSNParserBase::isBracket(char c) const
{
    return (c == '(') ? 1 : ((c == ')') ? -1 : 0);
}

/// Collect substrings for each element that looks like "(@name ... )" with matching brackets.
std::vector<std::string> DSNParserBase::gather(const std::string &s, const char *name, const int depth)
{
    // Just compute the nesting level for every position in the string in advance.
    std::vector<int8_t> level;
    level.resize(s.size() + 1, 0);
    for (uint i = 0; i < s.size(); ++i) {
        assert(level[i] >= 0 && level[i] < 127);
        level[i+1] = level[i] + checkBracket(s[i]);
    }
    assert(!mQuoteOpen && "string ends with unclosed quote");

    std::vector<std::string> v;
    const std::string pattern = std::string("(") + name;
    for (size_t pos = 0; pos < s.size();) {
        pos = s.find(pattern, pos);
        if (pos == std::string::npos)
            break;
        pos += pattern.size();
        if (level[pos] > depth)
            continue;
        if (!std::isspace(s[pos])) // space needs to follow pattern
            continue;
        pos = skipspace(s, pos);
        uint open = 1;
        size_t end = pos;
        for (; open && end < s.size(); ++end)
            open += checkBracket(s[end]);
        assert(!mQuoteOpen && "substring ends with unclosed quote");
        if (open)
            break;
        end = skipspaceRev(s, pos, end - 2);
        if (end + 1 > pos)
            v.push_back(s.substr(pos, end + 1 - pos));
    }
    return v;
}

// "(a) (b) (c)" -> ["a", "b", "c"]
std::vector<std::string> DSNParserBase::gatherArray(const std::string &s)
{
    std::vector<std::string> v;
    for (size_t pos = 0; pos < s.size();) {
        pos = s.find('(', pos);
        if (pos == std::string::npos)
            break;
        pos = skipspace(s, pos + 1);
        int open = 1;
        size_t end = pos;
        for (; open && end < s.size(); ++end)
            open += checkBracket(s[end]);
        assert(!mQuoteOpen && "array ends with unclosed quote");
        if (open)
            break;
        end = skipspaceRev(s, pos, end - 2);
        if (end + 1 > pos)
            v.push_back(s.substr(pos, end + 1 - pos));
    }
    return v;
}

std::string DSNParserBase::unquote(const std::string &sq) const
{
    // string(  "abc\"def"  ) -> string(abc\"def)
    std::string s;
    size_t a = skipspace(sq, 0);
    size_t b = skipspaceRev(sq, a, sq.size() - 1);
    if (a >= sq.size())
        return s;
    if ((sq[a] == mStringQuote) != (sq[b] == mStringQuote))
        throw std::runtime_error("tried to unquote semi-quoted string");
    if (sq[a] == mStringQuote && sq[b] == mStringQuote) { ++a; --b; }
    if (b >= a)
        s.reserve(b - a + 1);
    for (; a <= b; ++a)
        if (sq[a] != mEscapeChar)
            s += sq[a];
    return s;
}
std::string DSNParserBase::unquotePin(const std::string &sq)
{
    // string(IC1-"A-") -> string(IC1-A-)
    size_t sep = 0;
    while (sep < sq.size() && (checkQuote(sq[sep]) || sq[sep] != '-'))
        ++sep;
    resetQuote();
    if (sep < sq.size())
        return unquote(sq.substr(0, sep)) + "-" + unquote(sq.substr(sep + 1));
    return unquote(sq);
}

Token DSNParserBase::firstToken(const std::string &s, size_t *from)
{
    size_t pos = skipspace(s, from ? *from : 0);
    size_t end = pos;
    while (end < s.size() && (checkQuote(s[end]) || !std::isspace(s[end])))
        ++end;
    resetQuote();
    if (from)
        *from = end;
    return Token(s.substr(pos, end - pos));
}

std::vector<Token> DSNParserBase::tokenize(const std::string &_s)
{
    std::string s = _s.substr(0, nextBracket(_s, 1));
    while (!s.empty() && std::isspace(s.back()))
        s.pop_back();
    std::vector<Token> tokens;
    for (size_t pos = 0; pos < s.size();)
        tokens.push_back(firstToken(s, &pos));
    return tokens;
}


DSNParserBase::DSNParserBase(dsn::Data &data) : mData(data)
{
}

dsn::BBox dsn::get_bounding_box(const std::vector<double> &xy, double ex)
{
    assert(xy.size());
    dsn::BBox box;
    box.xmin = std::numeric_limits<double>::infinity();
    box.ymin = std::numeric_limits<double>::infinity();
    box.xmax = -box.xmin;
    box.ymax = -box.ymin;
    for (uint i = 0; i < (xy.size() & ~1); i += 2) {
        box.xmin = std::min(box.xmin, xy[i] - ex);
        box.ymin = std::min(box.ymin, xy[i+1] - ex);
        box.xmax = std::max(box.xmax, xy[i] + ex);
        box.ymax = std::max(box.ymax, xy[i+1] + ex);
    }
    return box;
}
dsn::BBox dsn::Path::bbox() const
{
    return get_bounding_box(xy, aperture_width);
}

void dsn::Shape::recenter()
{
    double X = 0.0;
    double Y = 0.0;
    for (uint i = 0; i < values.size(); i += 2) {
        X += values[i];
        Y += values[i+1];
    }
    X /= values.size() / 2;
    Y /= values.size() / 2;
    for (uint i = 0; i < values.size(); i += 2) {
        values[i]   -= X;
        values[i+1] -= Y;
    }
}

double dsn::Shape::maxExtent() const
{
    if (geometry == "circle")
        return values.at(0);
    const dsn::BBox box = (geometry == "path") ? path.bbox() : get_bounding_box(values, 0.0);
    return box.maxExtent();
}
double dsn::Shape::minExtent() const
{
    if (geometry == "circle")
        return values.at(0);
    const dsn::BBox box = (geometry == "path") ? path.bbox() : get_bounding_box(values, 0.0);
    return box.minExtent();
}

std::string dsn::PCB::str() const
{
    std::stringstream ss;
    ss << "PCB " << name << std::endl;
    ss << "unit: " << unit << 'e' << unit_ex << std::endl;
    ss << "resolution: " << resolution << 'e' << resolution_ex;
    return ss.str();
}
std::string dsn::Layer::str() const
{
    std::stringstream ss;
    ss << "layer " << kicad_index << ": " << name << " z=" << copper_index << ' ' << type << ' ' << side;
    return ss.str();
}
std::string dsn::Net::str() const
{
    std::stringstream ss;
    ss << "net " << name << " {";
    for (const auto &P : pins)
        ss << ' ' << P;
    ss << " }";
    return ss.str();
}
std::string dsn::Path::str(bool verbose) const
{
    std::stringstream ss;
    ss << "width=" << aperture_width << " z=" << z << " xy=double[" << xy.size() << ']';
    if (verbose) {
        for (uint i = 0; (i + 1) < xy.size(); i += 2)
            ss << " (" << xy[i] << ',' << xy[i+1] << ')';
    }
    return ss.str();
}
std::string dsn::Shape::str(bool verbose) const
{
    if (geometry == "path")
        return path.str();
    std::stringstream ss;
    ss << geometry << ' ' << z << " double[" << values.size() << ']';
    if (verbose)
        for (auto v : values)
            ss << ' ' << v;
    return ss.str();
}
std::string dsn::Padstack::str(bool verbose) const
{
    std::stringstream ss;
    ss << "padstack " << name << (attach ? " attach" : "");
    for (const auto &S : shapes)
        ss << std::endl << S.str(verbose);
    return ss.str();
}
std::string dsn::Pin::str() const
{
    std::stringstream ss;
    ss << "pin[" << index << "] \"" << name << "\" " << angle << "° (" << x << ',' << y << ',' << z0 << ':' << z1 << ") " << (padstack ? padstack->name : "N/A");
    return ss.str();
}
std::string dsn::Image::str() const
{
    std::stringstream ss;
    ss << "image " << name;
    if (!footprint.geometry.empty())
        ss << " footprint: " << footprint.str();
    for (const auto &P : pins)
        ss << std::endl << P.str();
    return ss.str();
}
std::string dsn::Structure::str() const
{
    std::stringstream ss;
    ss << "structure";
    for (const auto &I : width)
        ss << std::endl << "width " << I.second << ' ' << I.first;
    for (const auto &I : clearance)
        ss << std::endl << "clearance " << I.second << ' ' << I.first;
    for (const auto &L : layers)
        ss << std::endl << L.str();
    ss << std::endl << "boundary " << boundary.str(true);
    return ss.str();
}
std::string dsn::Placement::str() const
{
    std::stringstream ss;
    ss << "place " << name << ':' << (image ? image->name : "?") << " at " << x << ',' << y << ',' << z << " " << angle << "° pinref " << pinref_x << ',' << pinref_y;
    return ss.str();
}
std::string dsn::Data::str() const
{
    std::stringstream ss;
    ss << pcb.str() << std::endl;
    ss << structure.str();
    for (const auto &I : padstacks)
        ss << std::endl << I.second.str();
    for (const auto &I : images)
        ss << std::endl << I.second.str();
    for (const auto &I : placements)
        ss << std::endl << I.str();
    for (const auto &I : nets)
        ss << std::endl << I.second.str();
    return ss.str();
}

bool dsn::Path::operator==(const dsn::Path &that) const
{
    return aperture_width == that.aperture_width && z == that.z && xy == that.xy;
}
bool dsn::Shape::operator==(const dsn::Shape &that) const
{
    return geometry == that.geometry && values == that.values && path == that.path;
}

void dsn::Data::renamePinsWithSharedNames()
{
    std::map<std::string, std::list<std::string>> moreNames; // for adding new names to nets

    for (auto &C : placements)
        C.image->placements.push_back(&C);

    for (auto &I : images) {
        std::map<std::string, uint> counts;
        for (auto &P : I.second.pins) {
            const uint n = ++counts[P.name];
            if (n < 2)
                continue;
            const std::string extra = P.name + "§" + std::to_string(n - 1);
            for (const auto C : I.second.placements)
                moreNames[C->name + '-' + P.name].push_back(C->name + '-' + extra);
            P.compound = P.name;
            P.name = extra;
        }
    }
    for (const auto &L : moreNames)
        for (const auto &s : L.second)
            DEBUG("renamed shared pad " << L.first << " to " << s);

    for (auto &N : nets) {
        Net &net = N.second;
        std::list<std::string> more;
        for (const auto &pin : net.pins) {
            const auto L = moreNames.find(pin);
            if (L != moreNames.end())
                more.insert(more.end(), L->second.begin(), L->second.end());
        }
        net.pins.insert(more.begin(), more.end());
    }
}

void dsn::Data::inferMissingNetWidths()
{
    std::map<double, double> net2via;
    double min_width = std::numeric_limits<double>::infinity();
    for (auto &I : nets) {
        auto &N = I.second;
        double w = std::numeric_limits<double>::infinity();
        double d = std::numeric_limits<double>::infinity();
        if (N.width <= 0.0)
            for (const auto &s : N.segments)
                w = std::min(w, s.w);
        if (N.via_diameter <= 0.0)
            for (const auto &v : N.vias)
                d = std::min(d, v.dia);
        if (d <= 0.0 || w <= 0.0)
            throw std::runtime_error("segment or via via width <= 0 detected");
        if (!std::isinf(w))
            N.width = w;
        if (!std::isinf(d))
            N.via_diameter = d;
        if (N.width > 0.0)
            min_width = std::min(N.width, min_width);
        if (N.width > 0.0 && N.via_diameter > 0.0)
            net2via[N.width] = (net2via[N.width] > 0.0) ? std::min(net2via[N.width], N.via_diameter) : N.via_diameter;
    }
    if (std::isinf(min_width))
        throw std::runtime_error("no track widths can be inferred from data");
    for (auto &I : nets) {
        auto &N = I.second;
        if (N.width <= 0.0)
            N.width = min_width;
        if (N.via_diameter <= 0.0) {
            N.via_diameter = net2via[N.width];
            if (N.via_diameter <= 0.0)
                N.via_diameter = N.width * 2.0;
        }
        if (N.clearance <= 0.0)
            N.clearance = 0.5 * min_width;
    }
}

void dsn::Data::printNetWidths() const
{
    const double um = pcb.unit * std::pow(10.0, pcb.unit_ex + 6);
    for (auto &N : nets)
        INFO(N.second.name << ": T=" << (N.second.width * um) << " O=" << (N.second.via_diameter * um) << " C=" << (N.second.clearance * um));
}
