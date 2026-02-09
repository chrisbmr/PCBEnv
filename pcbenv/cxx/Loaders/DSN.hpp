#ifndef GYM_PCB_LOADERS_DSN_H
#define GYM_PCB_LOADERS_DSN_H

#include <map>
#include <set>
#include <string>
#include <vector>

namespace dsn {

struct Placement;

struct BBox
{
    double xmin;
    double ymin;
    double xmax;
    double ymax;
    double minExtent() const { return std::min(xmax - xmin, ymax - ymin); }
    double maxExtent() const { return std::max(xmax - xmin, ymax - ymin); }
};

BBox get_bounding_box(const std::vector<double> &xy, double expansion);

struct PCB
{
    std::string str() const;

    std::string name;
    int unit_ex;
    int resolution_ex;
    double unit{1.0};
    double resolution;
};

struct Layer
{
    std::string str() const;

    std::string name;
    std::string type;
    int copper_index{-1};
    int kicad_index{-1};
    char side{'m'};

    bool operator<(const Layer &L) const
    {
        if (side == 't' && L.side != 't')
            return true;
        if (side == 'b' && L.side != 'b')
            return false;
        if (side == 'i' && L.side != 'i')
            return L.side == 'b';
        if (kicad_index != L.kicad_index)
            return kicad_index < L.kicad_index;
        return name < L.name;
    }
};

struct Segment
{
    double x0, y0, x1, y1;
    double w;
    int z;
};

struct Via
{
    double dia;
    double drill;
    double x, y;
    int z0, z1;
};

struct Track
{
    double start_x, start_y;
    double end_x, end_y;
    int start_z;
    int end_z;
    double default_width;
    double default_via_diameter;
    std::vector<Segment> segments;
    std::vector<Via> vias;
};

struct Connection
{
    std::string src_pin;
    std::string dst_pin;
    bool has_src_pin{false};
    bool has_dst_pin{false};
    bool has_src{false};
    bool has_dst{false};
    double x0, y0;
    double x1, y1;
    int z0, z1;
    double clearance;
    double ref_len{0.0};
    std::vector<Track> tracks;
};

struct Net
{
    std::string str() const;

    std::string name;
    std::string klass;
    int index{-1};
    std::set<std::string> pins;
    double clearance{0.0};
    double via_diameter{0.0};
    double width{0.0};
    std::vector<Segment> segments;
    std::vector<Via> vias;
    std::vector<Connection> connections;
};

struct Path
{
    BBox bbox() const;
    bool operator==(const Path&) const;
    std::string str(bool verbose = false) const;

    double aperture_width{0.0};
    std::vector<double> xy;
    int z{-1};
};

struct Shape
{
    double minExtent() const;
    double maxExtent() const;
    bool operator==(const Shape&) const;
    std::string str(bool verbose = false) const;

    std::string geometry;
    std::vector<double> values; // circle stores diameter not radius
    int z;
    Path path;

    void recenter();
};

struct Padstack
{
    std::string str(bool verbose = false) const;

    std::string name;
    std::string type;
    std::vector<Shape> shapes;
    bool attach;
};

struct Pin
{
    std::string str() const;

    Padstack *padstack{0};
    std::string name;
    std::string net;
    int index;
    int angle{0};
    double x, y;
    int z0 = std::numeric_limits<int>::max();
    int z1 = -1;
    double clearance{0.0};
    bool has_net{false};
    std::optional<std::string> compound;
};

struct Image
{
    std::string str() const;

    std::string name;
    std::vector<Pin> pins;
    dsn::Shape footprint;

    std::vector<Placement *> placements;
};

struct Structure
{
    std::string str() const;

    std::vector<Layer *> copper_layers;
    std::vector<Layer> layers;
    Path boundary;
    std::map<std::string, std::string> via;
    std::map<std::string, double> width;
    std::map<std::string, double> clearance;
};

struct Placement
{
    std::string str() const;

    std::string name;
    dsn::Image *image{0};
    double x, pinref_x;
    double y, pinref_y;
    int z;
    int angle{0};
    double clearance{0};
    bool via_ok{true};
    bool can_route{false};
};

struct Data
{
    static bool parseDSN(dsn::Data&, const std::string&);
    static bool parseJSON(dsn::Data&, const std::string&);
    static bool parseKiCAD(dsn::Data&, const std::string&);
    std::string str() const;

    dsn::PCB pcb;
    dsn::Structure structure;
    std::map<std::string, dsn::Image> images;
    std::map<std::string, dsn::Padstack> padstacks;
    std::map<std::string, dsn::Net> nets;
    std::vector<dsn::Placement> placements;

    void renamePinsWithSharedNames();
    void inferMissingNetWidths();
    void printNetWidths() const;
};

} // namespace dsn

#endif // GYM_PCB_LOADERS_DSN_H
