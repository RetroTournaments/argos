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

#include "ext/opencvext/opencvext.h"

using namespace opencvext;

bool opencvext::CVMatsEqual(const cv::Mat Mat1, const cv::Mat Mat2)
{
    if (Mat1.dims != Mat2.dims ||
        Mat1.size != Mat2.size ||
        Mat1.elemSize() != Mat2.elemSize()) {
        return false;
    }

    if (Mat1.isContinuous() && Mat2.isContinuous()) {
        return 0 == memcmp(Mat1.ptr(), Mat2.ptr(), Mat1.total()*Mat1.elemSize());
    } else {
        const cv::Mat* arrays[] = {&Mat1, &Mat2, 0};
        uchar* ptrs[2];
        cv::NAryMatIterator it(arrays, ptrs, 2);
        for (unsigned int p = 0; p < it.nplanes; p++, ++it) {
            if (0 != memcmp(it.ptrs[0], it.ptrs[1], it.size*Mat1.elemSize())) {
                return false;
            }
        }

        return true;
    }
    return false;
}

cv::Mat opencvext::CropWithZeroPadding(cv::Mat img, cv::Rect cropRect)
{
    if (cropRect.width < 0) {
        cropRect.x += cropRect.width;
        cropRect.width = -cropRect.width;
    }
    if (cropRect.height < 0) {
        cropRect.y += cropRect.height;
        cropRect.height = -cropRect.height;
    }

    if (img.empty()) {
        return cv::Mat::zeros(cropRect.height, cropRect.width, CV_8UC3);
    }

    cv::Rect imgRect = cv::Rect(0, 0, img.cols, img.rows);
    cv::Rect overLapRect = imgRect & cropRect;
    if (overLapRect.width == cropRect.width && overLapRect.height == cropRect.height) {
        return img.clone()(overLapRect).clone();
    }

    // add padding still
    cv::Mat m = cv::Mat::zeros(cropRect.height, cropRect.width, img.type());
    if (overLapRect.width > 0 && overLapRect.height > 0) {
        img(overLapRect).copyTo(m(cv::Rect(overLapRect.x - cropRect.x, overLapRect.y - cropRect.y, overLapRect.width, overLapRect.height)));
    }
    return m;
}

cv::Mat opencvext::ResizePrefNearest(cv::Mat in, float scale)
{
    cv::Mat out = in;
    if (scale && scale != 1.0f) {
        float rndScale = std::round(scale);
        if (std::abs(scale - rndScale)  < 0.001f) {
            if (rndScale) {
                cv::resize(in, out, {}, rndScale, rndScale, cv::INTER_NEAREST);
            }
        } else {
            if (scale < 1.0f) {
                cv::resize(in, out, {}, scale, scale, cv::INTER_AREA);
            } else {
                cv::resize(in, out, {}, scale, scale);
            }
        }
    }

    return out;
}

cv::Mat opencvext::ConstructPaletteImage(
    const uint8_t* imgData,
    int width, int height,
    const uint8_t* paletteData,
    PaletteDataOrder paletteDataOrder
)
{
    cv::Mat m(height, width, CV_8UC3);

    uint8_t* o = reinterpret_cast<uint8_t*>(m.data);

    int a = 0;
    int b = 1;
    int c = 2;

    if (paletteDataOrder == PaletteDataOrder::RGB) {
        std::swap(a, c);
    }

    for (int k = 0; k < (width * height); k++) {
        const uint8_t* p = paletteData + (*imgData * 3);

        *(o + 0) = *(p + a);
        *(o + 1) = *(p + b);
        *(o + 2) = *(p + c);

        o += 3;
        imgData++;
    }
    return m;
}

cv::Mat opencvext::RGB565ToCVMat(const uint16_t* data, unsigned width, unsigned height, size_t pitch)
{
    static std::array<uint8_t, 0b00100000> RB_LOOKUP;
    static std::array<uint8_t, 0b01000000> G_LOOKUP;
    static bool LOOKUPS_INITIALIZED = false;

    if (!LOOKUPS_INITIALIZED) {
        float RB_FACTOR = 255.0f / 31.0f;
        float G_FACTOR = 255.0f / 63.0f;
        for (uint8_t r = 0x00; r < 0b00100000; r++) {
            float v = static_cast<float>(r);
            RB_LOOKUP[r] = static_cast<uint8_t>(std::round(v * RB_FACTOR));
        }

        for (uint8_t g = 0x00; g < 0b01000000; g++) {
            float v = static_cast<float>(g);
            G_LOOKUP[g] = static_cast<uint8_t>(std::round(v * G_FACTOR));
        }

        LOOKUPS_INITIALIZED = true;
    }

    if (pitch != width * 2) {
        throw std::invalid_argument("image format not supported yet because I am lazy opencvext::RGB565ToCVMat");
    }

    cv::Mat out(height, width, CV_8UC3);
    uint8_t* o = reinterpret_cast<uint8_t*>(out.data);

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            *o++ = RB_LOOKUP[(*data & 0b0000000000011111) >>  0];
            *o++ =  G_LOOKUP[(*data & 0b0000011111100000) >>  5];
            *o++ = RB_LOOKUP[(*data & 0b1111100000000000) >> 11];

            data++;
        }
    }

    return out;
}

cv::Mat opencvext::ResizeTo(cv::Mat img, int width, int height, cv::InterpolationFlags smaller, cv::InterpolationFlags larger)
{
    if (img.empty()) {
        return cv::Mat::zeros(height, width, CV_8UC3);
    }
    cv::InterpolationFlags v = smaller;
    if (width == img.cols && height == img.rows) {
        return img;
    } else if (width >= img.cols || height >= img.rows) {
        v = larger;
    }

    if (img.rows == 0 || img.cols == 0) {
        return cv::Mat::zeros(height, width, CV_8UC3);
    }

    cv::Mat out;
    cv::resize(img, out, {width, height}, 0, 0, v);
    return out;
}

