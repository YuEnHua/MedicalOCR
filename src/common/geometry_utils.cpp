#include "geometry_utils.h"

#include <algorithm>
#include <cmath>

namespace medical_ocr {
namespace geometry {

PointF BoxCenter(const BBox& box) {
    float cx = 0.0f;
    float cy = 0.0f;
    for (const auto& p : box) {
        cx += p.x;
        cy += p.y;
    }
    return {cx / 4.0f, cy / 4.0f};
}

void AxisAlignedBounds(const BBox& box,
                       float& x_min, float& y_min,
                       float& x_max, float& y_max) {
    x_min = box[0].x;
    y_min = box[0].y;
    x_max = box[0].x;
    y_max = box[0].y;
    for (const auto& p : box) {
        x_min = std::min(x_min, p.x);
        y_min = std::min(y_min, p.y);
        x_max = std::max(x_max, p.x);
        y_max = std::max(y_max, p.y);
    }
}

bool IsSameLine(const BBox& a, const BBox& b, float tolerance) {
    auto ca = BoxCenter(a);
    auto cb = BoxCenter(b);
    return std::fabs(ca.y - cb.y) <= tolerance;
}

float DistanceBetween(const BBox& a, const BBox& b) {
    auto ca = BoxCenter(a);
    auto cb = BoxCenter(b);
    float dx = ca.x - cb.x;
    float dy = ca.y - cb.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool IsRightOf(const BBox& a, const BBox& b, float tolerance) {
    float a_min_x, a_min_y, a_max_x, a_max_y;
    float b_min_x, b_min_y, b_max_x, b_max_y;
    AxisAlignedBounds(a, a_min_x, a_min_y, a_max_x, a_max_y);
    AxisAlignedBounds(b, b_min_x, b_min_y, b_max_x, b_max_y);
    // Box B starts after box A ends (with some tolerance)
    return b_min_x >= a_max_x - tolerance;
}

bool IsBelow(const BBox& a, const BBox& b, float tolerance) {
    float a_min_x, a_min_y, a_max_x, a_max_y;
    float b_min_x, b_min_y, b_max_x, b_max_y;
    AxisAlignedBounds(a, a_min_x, a_min_y, a_max_x, a_max_y);
    AxisAlignedBounds(b, b_min_x, b_min_y, b_max_x, b_max_y);
    // Box B starts below box A (with some tolerance)
    return b_min_y >= a_max_y - tolerance;
}

void NormalizeCoordinates(OcrResult& result, int target_size) {
    if (result.imageWidth <= 0 || result.imageHeight <= 0) return;

    float scale_x = static_cast<float>(target_size) / result.imageWidth;
    float scale_y = static_cast<float>(target_size) / result.imageHeight;

    for (auto& box : result.boxes) {
        for (auto& pt : box.points) {
            pt.x *= scale_x;
            pt.y *= scale_y;
        }
    }
    result.imageWidth = target_size;
    result.imageHeight = target_size;
}

}  // namespace geometry
}  // namespace medical_ocr
