#ifndef MEDICAL_OCR_GEOMETRY_UTILS_H_
#define MEDICAL_OCR_GEOMETRY_UTILS_H_

#include "medical_ocr/types.h"

namespace medical_ocr {
namespace geometry {

/**
 * Compute the center point of a bounding box.
 */
PointF BoxCenter(const BBox& box);

/**
 * Compute the axis-aligned bounding rectangle from a quadrilateral.
 * Returns {x_min, y_min, x_max, y_max} as floats.
 */
void AxisAlignedBounds(const BBox& box,
                       float& x_min, float& y_min,
                       float& x_max, float& y_max);

/**
 * Check if two text boxes are on the same horizontal line.
 *
 * @param a          First box.
 * @param b          Second box.
 * @param tolerance  Maximum vertical distance between box centers to be
 *                   considered on the same line (in pixels).
 * @return           true if the boxes are approximately on the same line.
 */
bool IsSameLine(const BBox& a, const BBox& b, float tolerance);

/**
 * Compute the Euclidean distance between two box centers.
 */
float DistanceBetween(const BBox& a, const BBox& b);

/**
 * Determine if box B is to the right of box A (with some tolerance).
 */
bool IsRightOf(const BBox& a, const BBox& b, float tolerance);

/**
 * Determine if box B is below box A (with some tolerance).
 */
bool IsBelow(const BBox& a, const BBox& b, float tolerance);

/**
 * Normalize coordinates from image pixel space to 0–1000 range.
 *
 * This ensures that template matching works across different image
 * resolutions.
 */
void NormalizeCoordinates(OcrResult& result, int target_size = 1000);

}  // namespace geometry
}  // namespace medical_ocr

#endif  // MEDICAL_OCR_GEOMETRY_UTILS_H_
