extern "C" {
#include <dpengine/tile.h>
}

#include "tile.h"

namespace drawdance {

Tile Tile::null()
{
    return Tile{nullptr};
}

Tile Tile::inc(DP_Tile *t)
{
    return Tile{DP_tile_incref_nullable(t)};
}

Tile Tile::noinc(DP_Tile *t)
{
    return Tile{t};
}

Tile Tile::fromColor(const QColor &color)
{
    return Tile::noinc(DP_tile_new_from_bgra(0, color.rgba()));
}

Tile::Tile(const Tile &other)
    : Tile{DP_tile_incref_nullable(other.m_data)}
{
}

Tile::Tile(Tile &&other)
    : Tile{other.m_data}
{
    other.m_data = nullptr;
}

Tile &Tile::operator=(const Tile &other)
{
    DP_tile_decref_nullable(m_data);
    m_data = DP_tile_incref_nullable(other.m_data);
    return *this;
}

Tile &Tile::operator=(Tile &&other)
{
    DP_tile_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

Tile::~Tile()
{
    DP_tile_decref_nullable(m_data);
}

DP_Tile *Tile::get() const
{
    return m_data;
}

bool Tile::isNull() const
{
    return !m_data;
}


QColor Tile::singleColor(const QColor &defaultValue)
{
    DP_Pixel15 pixel;
    if(DP_tile_same_pixel(m_data, &pixel)) {
		DP_UPixelFloat color = DP_upixel15_to_float(DP_pixel15_unpremultiply(pixel));
		return QColor::fromRgbF(color.r, color.g, color.b, color.a);
    } else {
        return defaultValue;
    }
}

Tile::Tile(DP_Tile *cs)
    : m_data{cs}
{
}

}
