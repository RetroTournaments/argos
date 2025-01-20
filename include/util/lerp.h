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

#ifndef STATIC_UTIL_LERP_HEADER
#define STATIC_UTIL_LERP_HEADER

namespace sta::util
{

// returns y
template <typename T>
T Lerp(T x, T x0, T x1, T y0, T y1) {
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}
template <typename T> // x already between 0.0 and 1.0
T Lerp2(T x, T y0, T y1) {
    return y0 + x * (y1 - y0);
}

////////////////////////////////////////////////////////////////////////////////
// Linspace
//
// std::vector<float> arr(100);
// InplaceLinspace(arr.begin(), arr.end(), 0, 14);
//
// for (auto v : linspace(0, 1, 100) {
// }
////////////////////////////////////////////////////////////////////////////////
template <class Iter, typename T>
void InplaceLinspaceBase(Iter begin, Iter end, T start, T stop) {
    typedef typename std::iterator_traits<Iter>::difference_type diff_t;
    typedef typename std::make_unsigned<diff_t>::type udiff_t;

    udiff_t n = end - begin;

    T delta = stop - start;
    T step = delta / static_cast<T>(n - 1);

    udiff_t i = 0;
    for (auto it = begin; it != end; ++it) {
        *it = start + i * step;
        i++;
    }
}

template <class Iter>
void InplaceLinspace(Iter begin, Iter end, float start, float stop) {
    InplaceLinspaceBase<Iter, float>(begin, end, start, stop);
}

// The linspace generator / iterator allows for the nice:
//
// for (auto v : linspace(0, 1, 100)) {
//     std::cout << v << std::endl;
// }
//
// without storing the entire thing in memory
template <typename T>
struct LinspaceIterator {
    LinspaceIterator(T start, T step, int i)
        : m_start(start)
        , m_step(step)
        , m_i(i)
    {}
    ~LinspaceIterator() {}

    bool operator!= (const LinspaceIterator& other) const {
        return m_i != other.m_i;
    }

    T operator* () const {
        return m_start + m_i * m_step;
    }

    const LinspaceIterator& operator++ () {
        ++m_i;
        return *this;
    }

    T m_start, m_step;
    int m_i;
};

// Generator for linspace
template <typename T>
struct LinspaceGeneratorBase {
    LinspaceGeneratorBase(T start, T stop, int n)
        : m_start(start)
        , m_n(n) {
        T delta = stop - start;
        m_step = delta / static_cast<T>(n - 1);
    }
    ~LinspaceGeneratorBase() {};

    LinspaceIterator<T> begin() const {
        return LinspaceIterator<T>(m_start, m_step, 0);
    }
    LinspaceIterator<T> end() const {
        return LinspaceIterator<T>(m_start, m_step, m_n);
    }

    T m_start, m_step;
    int m_n;
};
typedef LinspaceGeneratorBase<float> Linspace;

}

#endif
