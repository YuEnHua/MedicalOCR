#include "anchor_field_extractor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "src/common/geometry_utils.h"

namespace medical_ocr {

// ============================================================================
// Main Extraction
// ============================================================================

bool AnchorFieldExtractor::Extract(const std::vector<OcrTextBox>& boxes,
                                   const FieldRule& rule,
                                   ExtractedField& out_field) {
    if (boxes.empty() || rule.anchors.empty()) return false;

    // Step 1: Find the anchor text box.
    int anchor_idx = FindAnchorBox(boxes, rule.anchors);
    if (anchor_idx < 0) return false;

    // Step 2: Search for the value box.
    int value_idx = -1;
    ExtractionMethod method = ExtractionMethod::Default;

    if (rule.search_direction == "below") {
        value_idx = FindBelowBox(boxes, anchor_idx, rule.max_distance);
        method = ExtractionMethod::AnchorBelow;
    } else {
        // Default: search to the right on the same line.
        value_idx = FindRightBox(boxes, anchor_idx,
                                 rule.same_line_tolerance,
                                 rule.max_distance);
        method = ExtractionMethod::AnchorRight;
    }

    if (value_idx < 0) return false;

    // Step 3: Check region constraint (if specified).
    if (!rule.region.empty() && rule.region.size() == 4) {
        if (!IsInRegion(boxes[value_idx], rule.region)) {
            return false;
        }
    }

    // Step 4: Populate the output field.
    const auto& value_box = boxes[value_idx];
    out_field.value = value_box.text;
    out_field.raw_value = value_box.text;
    out_field.confidence = value_box.confidence;
    out_field.source_bbox = value_box.points;
    out_field.extraction_method = method;
    out_field.validation_status = ValidationStatus::NotValidated;

    return true;
}

// ============================================================================
// Find Anchor Box
// ============================================================================

int AnchorFieldExtractor::FindAnchorBox(
    const std::vector<OcrTextBox>& boxes,
    const std::vector<std::string>& anchors) {

    for (size_t i = 0; i < boxes.size(); ++i) {
        const std::string& text = boxes[i].text;
        for (const auto& anchor : anchors) {
            // The anchor text must appear in the box. For labels like "姓名：",
            // we match if the box contains "姓名" (with or without colon).
            if (text.find(anchor) != std::string::npos) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

// ============================================================================
// Find Right Box (same line)
// ============================================================================

int AnchorFieldExtractor::FindRightBox(
    const std::vector<OcrTextBox>& boxes,
    int anchor_idx,
    int same_line_tolerance,
    int max_distance) {

    const auto& anchor = boxes[anchor_idx];
    auto anchor_center = geometry::BoxCenter(anchor.points);

    int best_idx = -1;
    double best_distance = std::numeric_limits<double>::max();

    for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
        if (i == anchor_idx) continue;

        const auto& box = boxes[i];
        auto box_center = geometry::BoxCenter(box.points);

        // Must be on the same line (vertical tolerance).
        if (!geometry::IsSameLine(anchor.points, box.points,
                                  static_cast<float>(same_line_tolerance))) {
            continue;
        }

        // Must be to the right of the anchor.
        if (box_center.x <= anchor_center.x) continue;

        double dist = geometry::DistanceBetween(anchor.points, box.points);
        if (dist <= max_distance && dist < best_distance) {
            best_distance = dist;
            best_idx = i;
        }
    }

    return best_idx;
}

// ============================================================================
// Find Below Box
// ============================================================================

int AnchorFieldExtractor::FindBelowBox(
    const std::vector<OcrTextBox>& boxes,
    int anchor_idx,
    int max_distance) {

    const auto& anchor = boxes[anchor_idx];
    auto anchor_center = geometry::BoxCenter(anchor.points);

    int best_idx = -1;
    double best_distance = std::numeric_limits<double>::max();

    for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
        if (i == anchor_idx) continue;

        const auto& box = boxes[i];
        auto box_center = geometry::BoxCenter(box.points);

        // Must be below the anchor.
        if (box_center.y <= anchor_center.y) continue;

        // Horizontal proximity check: box should be roughly aligned
        // with the anchor (within a reasonable horizontal range).
        double h_dist = std::fabs(box_center.x - anchor_center.x);
        if (h_dist > max_distance * 2) continue;

        double dist = geometry::DistanceBetween(anchor.points, box.points);
        if (dist <= max_distance && dist < best_distance) {
            best_distance = dist;
            best_idx = i;
        }
    }

    return best_idx;
}

// ============================================================================
// Region Check
// ============================================================================

bool AnchorFieldExtractor::IsInRegion(const OcrTextBox& box,
                                      const std::vector<int>& region) {
    if (region.size() != 4) return true;

    auto center = geometry::BoxCenter(box.points);
    return center.x >= region[0] && center.x <= region[2] &&
           center.y >= region[1] && center.y <= region[3];
}

}  // namespace medical_ocr
