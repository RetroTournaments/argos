////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023 Matthew Deutsch
//
// Argos is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Argos is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Argos; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#ifndef ARGOS_UTIL_VECTOR_HEADER
#define ARGOS_UTIL_VECTOR_HEADER

#include <array>
#include <cstdint>

namespace argos::util
{

// The main base vector class
template <typename T, size_t n>
struct VectorBase {
    //static_assert(n >= 1, "Error: You many not initialize a vector with less than two dimensions");
    VectorBase() {};
    ~VectorBase() {};

    T& operator[](size_t idx) { return data[idx]; };
    const T& operator[](size_t idx) const { return data[idx]; };

    VectorBase& operator+=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] += rhs[i];
        return *this;
    }
    VectorBase& operator-=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] -= rhs[i];
        return *this;
    }
    VectorBase& operator*=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] *= rhs[i];
        return *this;
    }
    VectorBase& operator/=(const VectorBase& rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] /= rhs[i];
        return *this;
    }
    VectorBase& operator+=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] += rhs;
        return *this;
    }
    VectorBase& operator-=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] -= rhs;
        return *this;
    }
    VectorBase& operator*=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] *= rhs;
        return *this;
    }
    VectorBase& operator/=(T rhs) {
        for (size_t i = 0; i < n; i++)
            data[i] /= rhs;
        return *this;
    }
    VectorBase operator-() const {
        auto copy = *this;
        for (size_t i = 0; i < n; i++)
            copy[i] = -copy[i];
        return copy;
    }

    static VectorBase Origin() {
        VectorBase o;
        std::fill(o.data.begin(), o.data.end(), T(0));
        return o;
    }
    //constexpr static size_t Dimensions() {
    //    return n;
    //}

    std::array<T, n> data;
};
// Binary operations with another vector
template <typename T, size_t n>
inline VectorBase<T, n> operator+(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs += rhs; return lhs;
}
template <typename T, size_t n>
inline VectorBase<T, n> operator-(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs -= rhs; return lhs;
}
template <typename T, size_t n>
inline VectorBase<T, n> operator*(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs *= rhs; return lhs;
}
template <typename T, size_t n>
inline VectorBase<T, n> operator/(VectorBase<T, n> lhs, const VectorBase<T, n>& rhs) {
    lhs /= rhs; return lhs;
}
// Binary operations with a scalar
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator+(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs += static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator-(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs -= static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator*(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs *= static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator/(VectorBase<T, n> lhs, U rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    lhs /= static_cast<T>(rhs); return lhs;
}
template <typename T, size_t n, typename U>
inline VectorBase<T, n> operator*(U lhs, VectorBase<T, n> rhs) {
    static_assert(std::is_convertible<T, U>::value, "Error: Source type not convertible to destination type.");
    rhs *= lhs; return rhs;
}
// Equality operators
template <typename T, size_t n>
inline bool operator==(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return std::equal(lhs.data.begin(), lhs.data.end(), rhs.data.begin());
}
template <typename T, size_t n>
inline bool operator!=(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return !operator==(lhs, rhs);
}
// Lexicographic comparisons
template <typename T, size_t n>
inline bool operator< (const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    for (size_t i = 0; i < n - 1; i++)
        if (lhs[i] != rhs[i])
            return lhs[i] < rhs[i];
    return lhs[n - 1] < rhs[n - 1];
}
template <typename T, size_t n>
inline bool operator> (const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return operator<(rhs, lhs);
}
template <typename T, size_t n>
inline bool operator<=(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return !operator>(lhs, rhs);
}
template <typename T, size_t n>
inline bool operator>=(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    return !operator<(lhs, rhs);
}
// General Operations
template <typename T, size_t n>
inline T Distance(const VectorBase<T, n>& a, const VectorBase<T, n>& b) {
    return Magnitude(b - a);
}
template <typename T, size_t n>
inline T DistanceSquared(const VectorBase<T, n>& a, const VectorBase<T, n>& b) {
    return MagnitudeSquared(b - a);
}
template <typename T, size_t n>
inline T Dot(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    auto f1 = lhs.data.begin(); auto l1 = lhs.data.end();
    auto f2 = rhs.data.begin();
    T v = T(0);
    while (f1 != l1) {
        v += *f1 * *f2;
        ++f1; ++f2;
    }
    return v;
}
template <typename T, size_t n>
inline T MagnitudeSquared(const VectorBase<T, n>& vec) {
    return Dot(vec, vec);
}
template <typename T, size_t n>
inline T Magnitude(const VectorBase<T, n>& vec) {
    //static_assert(!std::is_integral<T>(), "Magnitude is invalid for integral types");
    return std::sqrt(MagnitudeSquared(vec));
}
template <typename T, size_t n>
inline T Theta(const VectorBase<T, n>& lhs, const VectorBase<T, n>& rhs) {
    //static_assert(!std::is_integral<T>(), "Theta is invalid for integral types");
    return std::acos(Dot(lhs, rhs) / (Magnitude(lhs) * Magnitude(rhs)));
}
template <typename T, size_t n>
inline VectorBase<T, n>& Normalize(VectorBase<T, n>& vec) {
    //static_assert(!std::is_integral<T>(), "Normalize is invalid for integral types");
    vec /= Magnitude(vec);
    return vec;
}
template <typename T, size_t n>
inline VectorBase<T, n>* Normalize(VectorBase<T, n>* vec) {
   (*vec) /= Magnitude(*vec);
   return vec;
}
template <typename T, size_t n>
inline VectorBase<T, n> Normalized(VectorBase<T, n> vec) {
    return Normalize(vec);
}
// 2D operations, higher dimensions are ignored
template <typename T, size_t n>
inline double TriArea2(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return ((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]));
}
template <typename T, size_t n>
inline double TriArea(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    //static_assert(!std::is_integral<T>(), "TriArea is invalid for integral types");
    return TriArea2(a, b, c) / 2.0;
}
template <typename T, size_t n>
inline bool IsLeft(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return TriArea2(a, b, c) > 0.0;
}
template <typename T, size_t n>
inline bool IsRight(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return TriArea2(a, b, c) < 0.0;
}
template <typename T, size_t n>
inline bool IsCollinear(const VectorBase<T, n>& a, const VectorBase<T, n>& b, const VectorBase<T, n>& c) {
    return TriArea2(a, b, c) == 0.0;
}

// Output
template <typename T, size_t n>
inline std::ostream& operator<<(std::ostream& os, const VectorBase<T, n>& vec) {
    os << "{";
    for (size_t i = 0; i < n - 1; i++)
        os << vec[i] << ", ";
    os << vec[n - 1] << "}";
    return os;
}
template <typename T, size_t n>
inline std::istream& operator>>(std::istream& is, VectorBase<T, n>& vec) {
    std::string str;
    std::getline(is, str, '{');
    for (size_t i = 0; i < n - 1; i++) {
        std::getline(is, str, ',');
        //vec[i] = std::stod(str);
        vec[i] = ::atof(str.c_str());
    }
    std::getline(is, str, '}');
    //vec[n - 1] = std::stod(str);
    vec[n - 1] = ::atof(str.c_str());
    return is;
}

// Specializations for the most common dimensions
template <typename T>
struct VectorBase<T, 3> {
    VectorBase() {};
    VectorBase(T x, T y, T z) : x(x), y(y), z(z) {};

    T& operator[](size_t idx) { return data[idx]; };
    const T& operator[](size_t idx) const { return data[idx]; };

    VectorBase& operator+=(const VectorBase& rhs) {
        x += rhs.x; y += rhs.y; z += rhs.z;
        return *this;
    }
    VectorBase& operator-=(const VectorBase& rhs) {
        x -= rhs.x; y -= rhs.y; z -= rhs.z;
        return *this;
    }
    VectorBase& operator*=(const VectorBase& rhs) {
        x *= rhs.x; y *= rhs.y; z *= rhs.z;
        return *this;
    }
    VectorBase& operator/=(const VectorBase& rhs) {
        x /= rhs.x; y /= rhs.y; z /= rhs.z;
        return *this;
    }
    VectorBase& operator+=(T rhs) {
        x += rhs; y += rhs; z += rhs;
        return *this;
    }
    VectorBase& operator-=(T rhs) {
        x -= rhs; y -= rhs; z -= rhs;
        return *this;
    }
    VectorBase& operator*=(T rhs) {
        x *= rhs; y *= rhs; z *= rhs;
        return *this;
    }
    VectorBase& operator/=(T rhs) {
        x /= rhs; y /= rhs; z /= rhs;
        return *this;
    }
    VectorBase operator-() const {
        return VectorBase(-x, -y, -z);
    }

    static VectorBase Origin() {
        return VectorBase(0.0, 0.0, 0.0);
    }
    static VectorBase XAxis() {
        return VectorBase(1.0, 0.0, 0.0);
    }
    static VectorBase YAxis() {
        return VectorBase(0.0, 1.0, 0.0);
    }
    static VectorBase ZAxis() {
        return VectorBase(0.0, 0.0, 1.0);
    }

    //constexpr static size_t Dimensions() {
    //    return 3;
    //}


    union {
        std::array<T, 3> data;
        struct { T x, y, z;};
        //VectorBase<T, 2> xy;
    };
};
template <typename T>
VectorBase<T, 3> inline Cross(const VectorBase<T, 3>& lhs, const VectorBase<T, 3>& rhs) {
    return VectorBase<T, 3>(lhs.y*rhs.z - lhs.z*rhs.y, lhs.z*rhs.x - lhs.x*rhs.z, lhs.x*rhs.y - lhs.y*rhs.x);
}

template <typename T>
struct VectorBase<T, 2> {
    VectorBase() {};
    VectorBase(VectorBase<T, 3> vec) : x(vec.x), y(vec.y) {};
    VectorBase(T x, T y) : x(x), y(y) {};

    T& operator[](size_t idx) { return data[idx]; };
    const T& operator[](size_t idx) const { return data[idx]; };

    VectorBase& operator+=(const VectorBase& rhs) {
        x += rhs.x; y += rhs.y;
        return *this;
    }
    VectorBase& operator-=(const VectorBase& rhs) {
        x -= rhs.x; y -= rhs.y;
        return *this;
    }
    VectorBase& operator*=(const VectorBase& rhs) {
        x *= rhs.x; y *= rhs.y;
        return *this;
    }
    VectorBase& operator/=(const VectorBase& rhs) {
        x /= rhs.x; y /= rhs.y;
        return *this;
    }
    VectorBase& operator+=(T rhs) {
        x += rhs; y += rhs;
        return *this;
    }
    VectorBase& operator-=(T rhs) {
        x -= rhs; y -= rhs;
        return *this;
    }
    VectorBase& operator*=(T rhs) {
        x *= rhs; y *= rhs;
        return *this;
    }
    VectorBase& operator/=(T rhs) {
        x /= rhs; y /= rhs;
        return *this;
    }
    VectorBase operator-() const {
        return VectorBase(-x, -y);
    }

    static VectorBase Origin() {
        return VectorBase(0.0, 0.0);
    }
    static VectorBase XAxis() {
        return VectorBase(1.0, 0.0);
    }
    static VectorBase YAxis() {
        return VectorBase(0.0, 1.0);
    }

    //constexpr static size_t Dimensions() {
    //    return 2;
    //}

    union {
        std::array<T, 2> data;
        struct { T x, y; };
    };
};

typedef VectorBase<double, 3> Vector3D;
typedef VectorBase<double, 2> Vector2D;
typedef VectorBase<float, 3> Vector3F;
typedef VectorBase<float, 2> Vector2F;
typedef VectorBase<long, 3> Vector3L;
typedef VectorBase<long, 2> Vector2L;
typedef VectorBase<int, 3> Vector3I;
typedef VectorBase<int, 2> Vector2I;

}

#endif

