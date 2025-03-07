////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023 Matthew Deutsch
//
// Static is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Static is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Static; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#ifndef STATIC_UTIL_RECT_HEADER
#define STATIC_UTIL_RECT_HEADER

#include "util/vector.h"

namespace sta::util
{

// x, y, width, height
template <typename T>
struct RectBase {
    RectBase() {};
    RectBase(T x, T y, T w, T h)
        : X(x)
        , Y(y)
        , Width(w)
        , Height(h)
    {
    }
    ~RectBase() {};

    T X, Y;
    T Width, Height;

    util::VectorBase<T, 2> TopLeft() const {
        return util::VectorBase<T, 2>(X, Y);
    }
    util::VectorBase<T, 2> BottomRight() const {
        return util::VectorBase<T, 2>(X + Width, Y + Height);
    }
    bool Overlaps(const util::VectorBase<T, 2>& pt) const {
        return pt.x >= X && pt.x <= (X + Width) && pt.y >= Y && pt.y <= (Y + Height);
    }
};

typedef RectBase<int> Rect2I;
typedef RectBase<float> Rect2F;

typedef std::array<Vector2F, 4> Quadrilateral2F;

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const RectBase<T>& rect) {
    os << "{" << rect.X << " " << rect.Y << " " << rect.Width << " " << rect.Height << "}";
    return os;
}

}

#endif

