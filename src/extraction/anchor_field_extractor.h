#ifndef MEDICAL_OCR_ANCHOR_FIELD_EXTRACTOR_H_
#define MEDICAL_OCR_ANCHOR_FIELD_EXTRACTOR_H_

#include <string>
#include <vector>

#include "medical_ocr/types.h"
#include "src/document/template_repository.h"

namespace medical_ocr {

/**
 * Extracts field values by finding anchor keywords and searching for
 * the closest text box to the right or below the anchor.
 *
 * All coordinates are expected in normalized 0–1000 space.
 */
class AnchorFieldExtractor {
public:
    /**
     * Extract a field value by finding the anchor keyword and searching
     * for the closest text box in the specified direction.
     *
     * @param boxes       All OCR text boxes (normalized coordinates).
     * @param rule        The field extraction rule from the template.
     * @param out_field   Output field with value, bbox, confidence, method.
     * @return            true if a value was found.
     */
    static bool Extract(const std::vector<OcrTextBox>& boxes,
                        const FieldRule& rule,
                        ExtractedField& out_field);

    /**
     * Find the index of the text box that best matches one of the anchor
     * keywords.
     *
     * @param boxes     OCR text boxes.
     * @param anchors   Anchor keyword list.
     * @return          Index of the best anchor box, or -1 if not found.
     */
    static int FindAnchorBox(const std::vector<OcrTextBox>& boxes,
                             const std::vector<std::string>& anchors);

    /**
     * Find the closest text box to the right of an anchor box, on the
     * same horizontal line.
     *
     * @param boxes              OCR text boxes.
     * @param anchor_idx         Index of the anchor box.
     * @param same_line_tolerance Vertical tolerance for "same line".
     * @param max_distance        Maximum search distance.
     * @return                    Index of the value box, or -1 if not found.
     */
    static int FindRightBox(const std::vector<OcrTextBox>& boxes,
                            int anchor_idx,
                            int same_line_tolerance,
                            int max_distance);

    /**
     * Find the closest text box below an anchor box.
     *
     * @param boxes         OCR text boxes.
     * @param anchor_idx    Index of the anchor box.
     * @param max_distance  Maximum search distance.
     * @return              Index of the value box, or -1 if not found.
     */
    static int FindBelowBox(const std::vector<OcrTextBox>& boxes,
                            int anchor_idx,
                            int max_distance);

    /**
     * Check if a text box is within a normalized region.
     *
     * @param box     The text box to check.
     * @param region  [x1, y1, x2, y2] in 0–1000 space.
     * @return        true if the box center is within the region.
     */
    static bool IsInRegion(const OcrTextBox& box,
                           const std::vector<int>& region);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_ANCHOR_FIELD_EXTRACTOR_H_
