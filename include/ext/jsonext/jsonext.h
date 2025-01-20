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

#ifndef EXT_JSONEXT_HEADER
#define EXT_JSONEXT_HEADER

#include "nlohmann/json.hpp"

// For a basic struct it is:
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(typename, member1, member2...);

#define JSONEXT_FROM_TO_CLASS(cname, fromfunc, tofunc)              \
inline void to_json(nlohmann::json& j, const cname& c) {            \
    j = c.tofunc();                                                 \
}                                                                   \
inline void from_json(const nlohmann::json& j, cname& c) {          \
    c.fromfunc(j);                                                  \
}

// Example:
//   enum class Friends {
//    EDGAR,
//    ARNIE,
//   };
//   NLOHMANN_JSON_SERIALIZE_ENUM(Friends, {
//    {Friends::EDGAR, "edgar"},
//    {Friends::ARNIE, "arnie"},
//   })
//   JSONEXT_SERIALIZE_ENUM_OPERATORS(Friends);
#define JSONEXT_SERIALIZE_ENUM_OPERATORS(etype)                     \
inline std::ostream& operator<<(std::ostream& os, const etype& e)   \
{                                                                   \
    nlohmann::json j = e;                                           \
    os << std::string(j);                                           \
    return os;                                                      \
}                                                                   \
inline std::istream& operator>>(std::istream& is, etype& e)         \
{                                                                   \
    std::string v;                                                  \
    is >> v;                                                        \
    nlohmann::json j = v;                                           \
    e = j.get<etype>();                                             \
    return is;                                                      \
}

#endif
