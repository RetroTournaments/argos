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

#include "util/clock.h"

using namespace argos::util;

mclock::time_point argos::util::Now()
{
    return mclock::now();
}

int64_t argos::util::ElapsedMillisFrom(const mclock::time_point& from)
{
    return ElapsedMillis(from, Now());
}

int64_t argos::util::ToMillis(const mclock::duration& duration)
{
    double sec = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    return static_cast<int64_t>(std::round(sec * 1000));
}

mclock::duration argos::util::ToDuration(int64_t millis)
{
    return std::chrono::milliseconds(millis);
}

int64_t argos::util::ElapsedMillis(const mclock::time_point& from, const mclock::time_point& to)
{
    return ToMillis(to - from);
}

std::string argos::util::GetTimestampNow()
{
    const auto t = std::chrono::system_clock::now();
    return fmt::format("{:%Y%m%dT%H%M%S}", t);
}

////////////////////////////////////////////////////////////////////////////////

std::string argos::util::SimpleDurationFormat(const mclock::duration& duration, SimpleTimeFormatFlags flags)
{
    return SimpleMillisFormat(ToMillis(duration), flags);
}

std::string argos::util::SimpleMillisFormat(int64_t millis, SimpleTimeFormatFlags flags)
{
    bool negative = millis < 0;
    millis = std::abs(millis);

    constexpr int64_t MILLIS_IN_SECONDS = 1000;
    constexpr int64_t MILLIS_IN_MINUTES = 60 * MILLIS_IN_SECONDS;
    constexpr int64_t MILLIS_IN_HOURS   = 60 * MILLIS_IN_MINUTES;
    constexpr int64_t MILLIS_IN_DAYS    = 24 * MILLIS_IN_HOURS;


    int64_t days = millis / MILLIS_IN_DAYS;
    millis -= days * MILLIS_IN_DAYS;
    int64_t hours = millis / MILLIS_IN_HOURS;
    millis -= hours * MILLIS_IN_HOURS;
    int64_t minutes = millis / MILLIS_IN_MINUTES;
    millis -= minutes * MILLIS_IN_MINUTES;
    int64_t seconds = millis / MILLIS_IN_SECONDS;
    millis -= seconds * MILLIS_IN_SECONDS;

    if (flags & SimpleTimeFormatFlags::CS) {
        if (millis >= 995) {
            seconds += 1;
            millis = 0;
        }
    }
    if (flags == SimpleTimeFormatFlags::MINS) {
        if (millis >= 500) {
            seconds += 1;
            millis = 0;
            if (seconds == 60) {
                minutes += 1;
                seconds = 0;
            }
        }
    }

    std::ostringstream os;
    if (negative) {
        os << "-";
    }

    auto OutNum = [&](int64_t num, int w) {
        os << std::setw(w) << std::setfill('0') << num;
    };
    if (flags & SimpleTimeFormatFlags::D) {
        OutNum(days, 2);
        os << ":";
    }
    if (flags & SimpleTimeFormatFlags::H) {
        if (flags & SimpleTimeFormatFlags::D) {
            OutNum(hours, 2);
        } else {
            OutNum(hours + days * 24, 1);
        }
        os << ":";
    }
    if (flags & SimpleTimeFormatFlags::M) {
        if (flags & SimpleTimeFormatFlags::H) {
            OutNum(minutes, 2);
        } else {
            OutNum(minutes + hours * 60 + days * 24, 1);
        }
        os << ":";
    }
    if (flags & SimpleTimeFormatFlags::S) {
        if (!(flags & SimpleTimeFormatFlags::MS) &&
            !(flags & SimpleTimeFormatFlags::CS)) {
            if (millis >= 500) {
                seconds += 1;
            }
        }
        OutNum(seconds, 2);
    }

    if (flags & SimpleTimeFormatFlags::MS) {
        os << ".";
        OutNum(millis, 3);
    }
    else if (flags & SimpleTimeFormatFlags::CS) {
        os << ".";
        int cs = static_cast<int>(std::round(static_cast<float>(millis) / 10));
        OutNum(cs, 2);
    }
    return os.str();
}

SimpleRateEstimator::SimpleRateEstimator(int64_t periodMillis, size_t periodCount)
    : m_Write(0)
    , m_WriteTime(Now())
    , m_PeriodMillis(periodMillis)
    , m_PastTicks(periodCount, 0.0)
{
    assert(periodMillis > 0);
    assert(periodCount > 1);
}

SimpleRateEstimator::~SimpleRateEstimator()
{
}

int64_t SimpleRateEstimator::GetPeriodMilliseconds() const
{
    return m_PeriodMillis;
}

double SimpleRateEstimator::TicksPerSecond()
{
    return TicksPerPeriod() * 1000.0 / static_cast<double>(m_PeriodMillis);
}

double SimpleRateEstimator::TicksPerPeriod()
{
    Tick(0.0);

    double denom = 0;
    double numer = 0;
    for (int i = 0; i < m_PastTicks.size(); i++) {
        if (i != m_Write) {
            numer += m_PastTicks[i];
            denom += 1.0;
        }
    }

    if (denom > 0.0) {
        return numer / denom;
    }
    return 0.0;
}

void SimpleRateEstimator::Reset()
{
    m_WriteTime = Now();
    m_PastTicks.assign(m_PastTicks.size(), 0.0);
    m_Write = 0;
}

void SimpleRateEstimator::Tick(double count)
{
    auto now = Now();
    int64_t elapsedMillis = ElapsedMillis(m_WriteTime, now);

    int64_t elapsedPeriods = elapsedMillis / m_PeriodMillis;
    m_WriteTime += ToDuration(elapsedPeriods * m_PeriodMillis);
    if (elapsedPeriods > m_PastTicks.size()) {
        Reset();
        elapsedPeriods = 0;
    }

    for (int64_t i = 0; i < elapsedPeriods; i++) {
        m_Write++;
        if (m_Write >= m_PastTicks.size()) {
            m_Write = 0;
        }
        m_PastTicks[m_Write] = 0.0;
    }

    m_PastTicks[m_Write] += count;
}

