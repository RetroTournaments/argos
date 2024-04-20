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

#ifndef ARGOS_UTIL_CLOCK_HEADER
#define ARGOS_UTIL_CLOCK_HEADER

#include <chrono>
#include <cstdint>
#include <vector>
#include <string>

namespace argos::util
{

using mclock = std::chrono::steady_clock;

mclock::time_point Now();
int64_t ToMillis(const mclock::duration& duration);
mclock::duration ToDuration(int64_t millis);
int64_t ElapsedMillis(const mclock::time_point& from, const mclock::time_point& to);
int64_t ElapsedMillisFrom(const mclock::time_point& from);

// Note simple in this context means: So simple as to be almost unusable
enum SimpleTimeFormatFlags : uint8_t {
    D    = 0b000001,
    H    = 0b000010,
    M    = 0b000100,
    S    = 0b001000,
    MS   = 0b010000,
    CS   = 0b100000,
    MSCS = 0b101100,
    HMS  = 0b001110,
    MINS = 0b001100,
};
std::string SimpleDurationFormat(const mclock::duration& duration, SimpleTimeFormatFlags flags);
std::string SimpleMillisFormat(int64_t millis, SimpleTimeFormatFlags flags);

std::string GetTimestampNow(); // %Y%m%dT%H%M%S

// for roughly calculating 'ticks' per second.
class SimpleRateEstimator
{
public:
    SimpleRateEstimator(int64_t periodMillis = 200, size_t periodCount = 32);
    ~SimpleRateEstimator();

    double TicksPerSecond(); // not const, because it will calculate the current rate
    double TicksPerPeriod();
    void Tick(double count = 1); // leave 1 for things like frames per second
    void Reset();

    int64_t GetPeriodMilliseconds() const;

private:
    int m_Write;
    mclock::time_point m_WriteTime;

    int64_t m_PeriodMillis;
    std::vector<double> m_PastTicks;
};

}

#endif
