#include "paragraph_extractor.h"

#include <algorithm>
#include <sstream>

#include "src/common/geometry_utils.h"

namespace medical_ocr {

// ============================================================================
// Main Extraction
// ============================================================================

bool ParagraphExtractor::Extract(const std::vector<OcrTextBox>& boxes,
                                 const FieldRule& rule,
                                 ExtractedField& out_field) {
    if (boxes.empty()) return false;

    // Step 1: Find start anchor.
    int start_idx = FindAnchorBox(boxes, rule.start_anchors);
    if (start_idx < 0) return false;

    // Step 2: Find end anchor (must appear after start anchor).
    int end_idx = -1;
    float start_y = geometry::BoxCenter(boxes[start_idx].points).y;

    // Search for end anchors that appear below the start anchor.
    for (const auto& end_anchor : rule.end_anchors) {
        for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
            if (boxes[i].text.find(end_anchor) != std::string::npos) {
                float box_y = geometry::BoxCenter(boxes[i].points).y;
                if (box_y > start_y) {
                    // Found a candidate end anchor below the start.
                    if (end_idx < 0 || box_y < geometry::BoxCenter(
                            boxes[end_idx].points).y) {
                        end_idx = i;
                    }
                }
            }
        }
    }

    if (end_idx < 0) return false;

    // Step 3: Collect all text boxes between start and end.
    std::vector<int> between_indices = CollectBetweenBoxes(
        boxes, start_idx, end_idx);

    if (between_indices.empty()) return false;

    // Step 4: Concatenate text.
    std::ostringstream oss;
    float avg_confidence = 0.0f;
    for (size_t i = 0; i < between_indices.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << boxes[between_indices[i]].text;
        avg_confidence += boxes[between_indices[i]].confidence;
    }
    avg_confidence /= between_indices.size();

    // Step 5: Populate output.
    out_field.value = oss.str();
    out_field.raw_value = out_field.value;
    out_field.confidence = avg_confidence;
    out_field.extraction_method = ExtractionMethod::ParagraphBetweenAnchors;
    out_field.validation_status = ValidationStatus::NotValidated;

    // Use the start anchor's bbox as the source.
    out_field.source_bbox = boxes[start_idx].points;

    return true;
}

// ============================================================================
// Find Anchor Box
// ============================================================================

int ParagraphExtractor::FindAnchorBox(
    const std::vector<OcrTextBox>& boxes,
    const std::vector<std::string>& anchors) {

    for (size_t i = 0; i < boxes.size(); ++i) {
        const std::string& text = boxes[i].text;
        for (const auto& anchor : anchors) {
            if (text.find(anchor) != std::string::npos) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

// ============================================================================
// Collect Between Boxes
// ============================================================================

std::vector<int> ParagraphExtractor::CollectBetweenBoxes(
    const std::vector<OcrTextBox>& boxes,
    int start_idx,
    int end_idx) {

    float start_y = geometry::BoxCenter(boxes[start_idx].points).y;
    float end_y = geometry::BoxCenter(boxes[end_idx].points).y;

    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
        if (i == start_idx || i == end_idx) continue;

        float box_y = geometry::BoxCenter(boxes[i].points).y;
        if (box_y > start_y && box_y < end_y) {
            result.push_back(i);
        }
    }

    // Sort by vertical position.
    std::sort(result.begin(), result.end(),
              [&](int a, int b) {
                  return geometry::BoxCenter(boxes[a].points).y <
                         geometry::BoxCenter(boxes[b].points).y;
              });

    return result;
}

}  // namespace medical_ocr
