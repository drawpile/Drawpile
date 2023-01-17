#ifndef DRAWDANCE_TILE_H
#define DRAWDANCE_TILE_H

extern "C" {
#include <dpengine/pixels.h>
}

#include <QColor>

struct DP_Tile;

namespace drawdance {

class Tile final {
public:
    static Tile null();
    static Tile inc(DP_Tile *t);
    static Tile noinc(DP_Tile *t);
    static Tile fromColor(const QColor &color);

    Tile(const Tile &other);
    Tile(Tile &&other);

    Tile &operator=(const Tile &other);
    Tile &operator=(Tile &&other);

    ~Tile();

    DP_Tile *get() const;

    bool isNull() const;

    // If the tile contains the same color for every pixel, this will return
    // that color (unpremultiplied.) If not, returns the given default value.
    QColor singleColor(const QColor &defaultValue);

private:
    explicit Tile(DP_Tile *t);

    DP_Tile *m_data;
};

}

#endif
