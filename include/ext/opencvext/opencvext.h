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

#ifndef EXT_OPENCVEXT_HEADER
#define EXT_OPENCVEXT_HEADER

#include "opencv2/opencv.hpp"

namespace opencvext
{

bool CVMatsEqual(const cv::Mat im1, const cv::Mat im2);

cv::Mat CropWithZeroPadding(cv::Mat img, cv::Rect r);
cv::Mat ResizePrefNearest(cv::Mat img, float scale);
cv::Mat ResizeTo(cv::Mat img, int width, int height,
        cv::InterpolationFlags smaller = cv::INTER_AREA,
        cv::InterpolationFlags larger = cv::INTER_LINEAR);

enum class PaletteDataOrder {
    RGB,
    BGR
};
cv::Mat ConstructPaletteImage(
    const uint8_t* imgData,
    int width, int height,
    const uint8_t* paletteData,
    PaletteDataOrder paletteDataOrder = PaletteDataOrder::RGB
);

cv::Mat RGB565ToCVMat(const uint16_t* data, unsigned width, unsigned height, size_t pitch);

}

#endif

