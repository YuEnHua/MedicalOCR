#ifndef MEDICAL_OCR_PARAGRAPH_EXTRACTOR_H_
#define MEDICAL_OCR_PARAGRAPH_EXTRACTOR_H_

#include <string>
#include <vector>

#include "medical_ocr/types.h"
#include "src/document/template_repository.h"

namespace medical_ocr {

/**
 * Extracts a paragraph of text between a start anchor and an end anchor.
 *
 * Used for fields like "findings" (检查所见 → 诊断意见) and
 * "impression" (诊断意见 → 报告医生).
 */
class ParagraphExtractor {
public:
    /**
     * Extract all text between a start anchor and an end anchor.
     *
     * @param boxes   All OCR text boxes (sorted by position).
     * @param rule    The field rule with start_anchors and end_anchors.
     * @param out_field  Output field with concatenated text.
     * @return        true if text was extracted.
     */
    static bool Extract(const std::vector<OcrTextBox>& boxes,
                        const FieldRule& rule,
                        ExtractedField& out_field);

    /**
     * Find the text box index for any of the given anchor keywords.
     *
     * @param boxes    OCR text boxes.
     * @param anchors  List of anchor keywords to search for.
     * @return         Index of the first matching box, or -1.
     */
    static int FindAnchorBox(const std::vector<OcrTextBox>& boxes,
                             const std::vector<std::string>& anchors);

    /**
     * Collect all text boxes between start_idx and end_idx (exclusive).
     * Boxes are sorted by vertical position.
     */
    static std::vector<int> CollectBetweenBoxes(
        const std::vector<OcrTextBox>& boxes,
        int start_idx,
        int end_idx);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_PARAGRAPH_EXTRACTOR_H_
