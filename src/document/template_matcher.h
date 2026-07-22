#ifndef MEDICAL_OCR_TEMPLATE_MATCHER_H_
#define MEDICAL_OCR_TEMPLATE_MATCHER_H_

#include <string>
#include <vector>

#include "medical_ocr/types.h"
#include "src/document/template_repository.h"

namespace medical_ocr {

/**
 * Result of template matching against OCR output.
 */
struct TemplateMatchResult {
    /// The matched template (empty template_id if no match).
    ReportTemplate matched_template;

    /// Match score — higher is better.
    int score = 0;

    /// Whether the minimum required keywords were all found.
    bool all_required_found = false;

    /// How many hospital keywords were matched.
    int hospital_hits = 0;

    /// How many title keywords were matched.
    int title_hits = 0;

    /// The concatenated OCR text used for matching.
    std::string all_text;
};

/**
 * Matches OCR output against loaded report templates.
 *
 * Algorithm:
 * 1. Concatenate all OCR text boxes into a single string.
 * 2. For each loaded template:
 *    a. Check that ALL required_keywords are present.
 *    b. Count hospital_keyword matches.
 *    c. Count title_keyword matches.
 *    d. Count optional_keyword matches.
 * 3. Score = sum of all matches; weight by keyword category.
 * 4. Return the highest-scoring template, or null match.
 */
class TemplateMatcher {
public:
    /**
     * Find the best matching template for an OCR result.
     *
     * @param ocr_result  The OCR output.
     * @param repository  The template repository to search.
     * @return            Match result. Check matched_template.template_id
     *                    — if empty, no template matched.
     */
    TemplateMatchResult Match(const OcrResult& ocr_result,
                              const TemplateRepository& repository) const;

    /**
     * Concatenate all text boxes from an OcrResult into a single string,
     * separated by newlines.
     */
    static std::string ConcatenateText(const OcrResult& ocr_result);

    /**
     * Score a single template against the OCR text.
     */
    static int ScoreTemplate(const ReportTemplate& tmpl,
                             const std::string& all_text,
                             bool& all_required_found,
                             int& hospital_hits,
                             int& title_hits);

    /**
     * Count how many times any keyword from a list appears in the text.
     */
    static int CountKeywordHits(const std::vector<std::string>& keywords,
                                const std::string& text);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_TEMPLATE_MATCHER_H_
