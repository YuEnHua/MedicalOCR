#ifndef MEDICAL_OCR_DOCUMENT_CLASSIFIER_H_
#define MEDICAL_OCR_DOCUMENT_CLASSIFIER_H_

#include <string>
#include <vector>

#include "medical_ocr/types.h"

namespace medical_ocr {

/**
 * Rule-based document type classifier.
 *
 * Scans OCR text for hospital names, report titles, and document-type
 * keywords to classify the report type.
 *
 * Does NOT use machine learning — purely keyword-based.
 */
class DocumentClassifier {
public:
    /**
     * Classify a document based on OCR text content.
     *
     * @param all_text  Concatenated OCR text from all boxes (UTF-8).
     * @return          The detected document type.
     */
    static DocumentType Classify(const std::string& all_text);

    /**
     * Get the list of keywords associated with each document type.
     */
    static const std::vector<std::string>& GetKeywords(DocumentType type);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_DOCUMENT_CLASSIFIER_H_
