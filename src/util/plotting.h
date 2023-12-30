////////////////////////////////////////////////////////////////////////////////

#ifndef RGM_UTIL_PLOTTING_HEADER
#define RGM_UTIL_PLOTTING_HEADER

#include <functional>

namespace rgms::util
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

