extern "C" {
#include <dpengine/tile.h>
}

#include "tile.h"

namespace drawdance {

Tile Tile::inc(DP_Tile *t)
{
    return Tile{DP_tile_incref_nullable(t)};
}

Tile Tile::noinc(DP_Tile *t)
{
    return Tile{t};
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

bool Tile::isNull() const
{
    return !m_data;
}


bool Tile::samePixel(DP_Pixel15 *outPixel) const
{
    return DP_tile_same_pixel(m_data, outPixel);
}

Tile::Tile(DP_Tile *cs)
    : m_data{cs}
{
}

}
