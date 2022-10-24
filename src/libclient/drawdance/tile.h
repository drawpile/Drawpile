#ifndef DRAWDANCE_TILE_H
#define DRAWDANCE_TILE_H

extern "C" {
#include <dpengine/pixels.h>
}

struct DP_Tile;

namespace drawdance {

class Tile final {
public:
    static Tile inc(DP_Tile *t);
    static Tile noinc(DP_Tile *t);

    Tile(const Tile &other);
    Tile(Tile &&other);

    Tile &operator=(const Tile &other);
    Tile &operator=(Tile &&other);

    ~Tile();

    bool isNull() const;

    bool samePixel(DP_Pixel15 *outPixel = nullptr) const;

private:
    explicit Tile(DP_Tile *t);

    DP_Tile *m_data;
};

}

#endif
