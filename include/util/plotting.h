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

#ifndef STATIC_UTIL_PLOTTING_HEADER
#define STATIC_UTIL_PLOTTING_HEADER

#include <functional>

namespace sta::util
{

////////////////////////////////////////////////////////////////////////////////
// Drawing stuff
////////////////////////////////////////////////////////////////////////////////
typedef std::function<void(int, int, double)> AAPlotFunc;
typedef std::function<void(int, int)> PlotFunc;

void HorizontalLine(int x0, int y, int x1, PlotFunc plot);
void VerticalLine(int x, int y0, int y1, PlotFunc plot);
void BresenhamLine(int x0, int y0, int x1, int y1, PlotFunc plot);
void WuLine(double x0, double y0, double x1, double y1, AAPlotFunc plot);

void BresenhamCircle(int x, int y, int rad, PlotFunc plot);
void BresenhamCircleFill(int x, int y, int rad, PlotFunc plot);
void BresenhamEllipse(int x, int y, int width, int height, PlotFunc plot);
void BresenhamEllipseFill(int x, int y, int width, int height, PlotFunc plot);

bool LiangBarsky(double xmin, double ymin, double xmax, double ymax,
                 double* x0, double* y0, double* x1, double* y1);

}

#endif

