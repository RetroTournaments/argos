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

#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <iostream>

#include "util/plotting.h"

using namespace sta::util;

void sta::util::HorizontalLine(int x0, int y, int x1, PlotFunc plot)
{
    if (x1 < x0) std::swap(x0, x1);
    for (int x = x0; x <= x1; x++) {
        plot(x, y);
    }
}
void sta::util::VerticalLine(int x, int y0, int y1, PlotFunc plot)
{
    if (y1 < y0) std::swap(y0, y1);
    for (int y = y0; y <= y1; y++) {
        plot(x, y);
    }
}

static int Octant(int dx, int dy)
{
    if (dx > 0) {
        if (dy > 0) {
            return (dy > dx) ? 1 : 0;
        } else {
            return (-dy > dx) ? 6 : 7;
        }
    } else {
        if (dy > 0) {
            return (dy > -dx) ? 2 : 3;
        } else {
            return (-dy > -dx) ? 5 : 4;
        }
    }
}

static void ToOctant(int octant, int x, int y, int* xp, int* yp)
{
    switch (octant) {
        case 0: { *xp =  x; *yp =  y; } break;
        case 1: { *xp =  y; *yp =  x; } break;
        case 2: { *xp =  y; *yp = -x; } break;
        case 3: { *xp = -x; *yp =  y; } break;
        case 4: { *xp = -x; *yp = -y; } break;
        case 5: { *xp = -y; *yp = -x; } break;
        case 6: { *xp = -y; *yp =  x; } break;
        case 7: { *xp =  x; *yp = -y; } break;
        default: {} break;
    }
}

static void FromOctant(int octant, int x, int y, int* xp, int* yp)
{
    switch (octant) {
        case 0: { *xp =  x; *yp =  y; } break;
        case 1: { *xp =  y; *yp =  x; } break;
        case 2: { *xp = -y; *yp =  x; } break;
        case 3: { *xp = -x; *yp =  y; } break;
        case 4: { *xp = -x; *yp = -y; } break;
        case 5: { *xp = -y; *yp = -x; } break;
        case 6: { *xp =  y; *yp = -x; } break;
        case 7: { *xp =  x; *yp = -y; } break;
        default: {} break;
    }
}

// Only in the zeroth octive
static void BresenhamLine0(int x0, int y0, int x1, int y1, PlotFunc plot)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int D = 2 * dy - dx;
    int y = y0;

    if (dx <= 0 || dy <= 0 || dy > dx) {
        std::cout << "error" << std::endl;
    }

    for (int x = x0; x <= x1; x++) {
        plot(x, y);
        if (D > 0) {
            y = y + 1;
            D = D - 2 * dx;
        }
        D = D + 2 * dy;
    }
}

void sta::util::BresenhamLine(int x0, int y0, int x1, int y1, PlotFunc plot)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    if (dx == 0 && dy == 0) {
        plot(x0, y0);
        return;
    } else if (dx == 0) {
        if (y1 < y0) std::swap(y0, y1);
        for (int y = y0; y <= y1; y++) {
            plot(x0, y);
        }
        return;
    } else if (dy == 0) {
        if (x1 < x0) std::swap(x0, x1);
        for (int x = x0; x <= x1; x++) {
            plot(x, y0);
        }
        return;
    }

    int octant = Octant(dx, dy);

    int x0p, y0p, x1p, y1p;
    ToOctant(octant, x0, y0, &x0p, &y0p);
    ToOctant(octant, x1, y1, &x1p, &y1p);
    BresenhamLine0(x0p, y0p, x1p, y1p, [&](int x, int y){
        int xp, yp;
        FromOctant(octant, x, y, &xp, &yp);
        plot(xp, yp);
    });
}

void sta::util::BresenhamCircle(int x, int y, int rad, PlotFunc plot)
{
    int xi = rad;
    int yi = 0;
    int dx = 1 - 2 * rad;
    int dy = 1;
    int err = 0;

    while (xi >= yi)
    {
        plot(x + xi, y + yi);
        plot(x + xi, y - yi);
        plot(x - xi, y + yi);
        plot(x - xi, y - yi);
        plot(x + yi, y + xi);
        plot(x + yi, y - xi);
        plot(x - yi, y + xi);
        plot(x - yi, y - xi);

        yi++;
        err += dy;
        dy += 2;
        if ((2 * err + dx) > 0) {
            xi--;
            err += dx;
            dx += 2;
        }
    }
}

void sta::util::BresenhamCircleFill(int x, int y, int rad, PlotFunc plot)
{
    int xi = rad;
    int yi = 0;
    int dx = 1 - 2 * rad;
    int dy = 1;
    int err = 0;

    bool change = false;
    while (xi >= yi)
    {
        HorizontalLine(x - xi, y - yi, x + xi, plot);
        if (yi) HorizontalLine(x - xi, y + yi, x + xi, plot);

        yi++;
        err += dy;
        dy += 2;
        if ((2 * err + dx) > 0) {
            if (xi + 1 != yi) {
                HorizontalLine(x - yi, y + xi, x + yi, plot);
                HorizontalLine(x - yi, y - xi, x + yi, plot);
            }

            xi--;
            err += dx;
            dx += 2;
        }
    }
}

void sta::util::BresenhamEllipse(int x, int y, int width, int height, PlotFunc plot)
{
    int a2 = width * width;
    int b2 = height * height;
    int fa2 = 4 * a2;
    int fb2 = 4 * b2;
    int xi, yi, d;

    for (xi = 0, yi = height, d = 2 * b2 + a2 * (1 - 2 * height);
         b2 * xi <= a2 * yi; xi++) {
        plot(x + xi, y + yi);
        plot(x - xi, y + yi);
        plot(x + xi, y - yi);
        plot(x - xi, y - yi);
        if (d >= 0) {
            d += fa2 * (1 - yi);
            yi--;
        }
        d += b2 * ((4 * xi) + 6);
    }

    for (xi = width, yi = 0, d = 2 * a2 + b2 * (1 - 2 * width);
         a2 * yi <= b2 * xi; yi++) {
        plot(x + xi, y + yi);
        plot(x - xi, y + yi);
        plot(x + xi, y - yi);
        plot(x - xi, y - yi);
        if (d >= 0) {
            d += fb2 * (1 - xi);
            xi--;
        }
        d += a2 * ((4 * yi) + 6);
    }
}

void sta::util::BresenhamEllipseFill(int x, int y, int width, int height, PlotFunc plot)
{
    int a2 = width * width;
    int b2 = height * height;
    int fa2 = 4 * a2;
    int fb2 = 4 * b2;
    int xi, yi, d;

    for (xi = 0, yi = height, d = 2 * b2 + a2 * (1 - 2 * height);
         b2 * xi <= a2 * yi; xi++) {

        if (d >= 0) {
            HorizontalLine(x - xi, y + yi, x + xi, plot);
            HorizontalLine(x - xi, y - yi, x + xi, plot);

            d += fa2 * (1 - yi);
            yi--;
        }
        d += b2 * ((4 * xi) + 6);
    }

    for (xi = width, yi = 0, d = 2 * a2 + b2 * (1 - 2 * width);
         a2 * yi <= b2 * xi; yi++) {
        HorizontalLine(x - xi, y + yi, x + xi, plot);
        if (yi) HorizontalLine(x - xi, y - yi, x + xi, plot);

        if (d >= 0) {
            d += fb2 * (1 - xi);
            xi--;
        }
        d += a2 * ((4 * yi) + 6);
    }
}


#define NEAR_ZERO(x) (((x) < 0.0000001) && ((x) > -0.0000001))
static bool ClipT(double num, double denom, double* tE, double* tL)
{
    if (NEAR_ZERO(denom)) return (num <= 0.0);
    double t = num / denom;
    if (denom > 0) {
        if (t > *tL) return false;
        if (t > *tE) *tE = t;
    } else {
        if (t < *tE) return false;
        if (t < *tL) *tL = t;
    }
    return true;
}
bool sta::util::LiangBarsky(double xmin, double ymin, double xmax, double ymax,
                 double* x0, double* y0, double* x1, double* y1)
{
    double dx = *x1 - *x0;
    double dy = *y1 - *y0;

    double tE = 0;
    double tL = 1;

    if (ClipT(xmin - *x0,  dx, &tE, &tL) &&
        ClipT(*x0 - xmax, -dx, &tE, &tL) &&
        ClipT(ymin - *y0,  dy, &tE, &tL) &&
        ClipT(*y0 - ymax, -dy, &tE, &tL))
    {
        if (tL < 1) {
            *x1 = *x0 + tL * dx;
            *y1 = *y0 + tL * dy;
        }
        if (tE > 0) {
            *x0 += tE * dx;
            *y0 += tE * dy;
        }
        return true;
    }
    return false;
}
#undef NEAR_ZERO

#define  IPART(x) (static_cast<int>(x))
#define  ROUND(x) (static_cast<int>(x + 0.5))
#define  FPART(x) ((x < 0) ? 1 - IPART(x) : x - IPART(x))
#define RFPART(x) (1 - FPART(x))
void sta::util::WuLine(double x0, double y0, double x1, double y1, AAPlotFunc plot)
{
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    double dx = x1 - x0;
    double dy = y1 - y0;
    double gradient = (dx == 0) ? 1.0 : dy / dx;

    int xend = ROUND(x0);
    double yend = y0 + gradient * (xend - x0);
    double xgap = RFPART(x0 + 0.5);
    int xpxl1 = xend;
    int ypxl1 = IPART(yend);
    if (steep) {
        plot(    ypxl1, xpxl1, RFPART(yend) * xgap);
        plot(ypxl1 + 1, xpxl1,  FPART(yend) * xgap);
    } else {
        plot(    xpxl1, ypxl1, RFPART(yend) * xgap);
        plot(xpxl1, ypxl1 + 1,  FPART(yend) * xgap);
    }
    double intery = yend + gradient;

    xend = ROUND(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = FPART(x1 + 0.5);
    int xpxl2 = xend;
    int ypxl2 = IPART(yend);
    if (steep) {
        plot(    ypxl2, xpxl2, RFPART(yend) * xgap);
        plot(ypxl2 + 1, xpxl2,  FPART(yend) * xgap);
    } else {
        plot(    xpxl2, ypxl2, RFPART(yend) * xgap);
        plot(xpxl2, ypxl2 + 1,  FPART(yend) * xgap);
    }

    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            plot(IPART(intery)    , x, RFPART(intery));
            plot(IPART(intery) + 1, x,  FPART(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            plot(x, IPART(intery)    , RFPART(intery));
            plot(x, IPART(intery) + 1,  FPART(intery));
            intery += gradient;
        }
    }
}
#undef IPART
#undef ROUND
#undef FPART
#undef RFPART

