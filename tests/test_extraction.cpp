#include <vector>

#include "gtest/gtest.h"
#include "src/common/geometry_utils.h"
#include "src/extraction/anchor_field_extractor.h"
#include "src/extraction/paragraph_extractor.h"
#include "src/extraction/field_extractor.h"
#include "src/document/template_repository.h"
#include "src/document/template_matcher.h"

namespace medical_ocr {
namespace {

// Helper: create a text box at a position.
static OcrTextBox MakeBox(const std::string& text, float x, float y,
                          float w, float h, float conf = 0.98f) {
    OcrTextBox box;
    box.text = text;
    box.confidence = conf;
    box.points = {{
        {x, y},
        {x + w, y},
        {x + w, y + h},
        {x, y + h}
    }};
    return box;
}

// ============================================================================
// AnchorFieldExtractor
// ============================================================================

TEST(AnchorFieldExtractorTest, FindAnchorBox_Found) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("姓名：", 100, 100, 120, 30),
        MakeBox("张三", 250, 100, 80, 30),
    };
    int idx = AnchorFieldExtractor::FindAnchorBox(boxes, {"姓名"});
    EXPECT_EQ(idx, 0);
}

TEST(AnchorFieldExtractorTest, FindAnchorBox_NotFound) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("Hello", 100, 100, 120, 30),
    };
    int idx = AnchorFieldExtractor::FindAnchorBox(boxes, {"姓名"});
    EXPECT_EQ(idx, -1);
}

TEST(AnchorFieldExtractorTest, FindRightBox_OnSameLine) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("姓名：", 100, 100, 120, 30),
        MakeBox("张三", 250, 100, 80, 30),
        MakeBox("Other", 100, 500, 80, 30),  // Far away.
    };
    int idx = AnchorFieldExtractor::FindRightBox(boxes, 0, 30, 300);
    EXPECT_EQ(idx, 1);
}

TEST(AnchorFieldExtractorTest, FindRightBox_NotOnSameLine) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("姓名：", 100, 100, 120, 30),
        MakeBox("不在同一行", 250, 200, 120, 30),  // Different line.
    };
    int idx = AnchorFieldExtractor::FindRightBox(boxes, 0, 20, 300);
    EXPECT_EQ(idx, -1);
}

TEST(AnchorFieldExtractorTest, FindRightBox_TooFar) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("姓名：", 100, 100, 120, 30),
        MakeBox("太远了", 800, 100, 80, 30),  // Beyond max_distance.
    };
    int idx = AnchorFieldExtractor::FindRightBox(boxes, 0, 30, 100);
    EXPECT_EQ(idx, -1);
}

TEST(AnchorFieldExtractorTest, FindBelowBox) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("标题", 100, 100, 120, 30),
        MakeBox("值在下方", 120, 150, 120, 30),
    };
    int idx = AnchorFieldExtractor::FindBelowBox(boxes, 0, 200);
    EXPECT_EQ(idx, 1);
}

TEST(AnchorFieldExtractorTest, Extract_AnchorRight) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("姓名：", 100, 100, 120, 30),
        MakeBox("张三", 250, 100, 80, 30),
    };
    FieldRule rule;
    rule.anchors = {"姓名"};
    rule.search_direction = "right";
    rule.same_line_tolerance = 20;
    rule.max_distance = 250;

    ExtractedField field;
    bool ok = AnchorFieldExtractor::Extract(boxes, rule, field);
    EXPECT_TRUE(ok);
    EXPECT_EQ(field.value, "张三");
    EXPECT_EQ(field.extraction_method, ExtractionMethod::AnchorRight);
}

TEST(AnchorFieldExtractorTest, Extract_AnchorNotFound) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("Hello", 100, 100, 120, 30),
    };
    FieldRule rule;
    rule.anchors = {"姓名"};

    ExtractedField field;
    bool ok = AnchorFieldExtractor::Extract(boxes, rule, field);
    EXPECT_FALSE(ok);
}

TEST(AnchorFieldExtractorTest, IsInRegion_Inside) {
    auto box = MakeBox("test", 500, 500, 100, 30);
    EXPECT_TRUE(AnchorFieldExtractor::IsInRegion(box, {0, 0, 1000, 1000}));
}

TEST(AnchorFieldExtractorTest, IsInRegion_Outside) {
    auto box = MakeBox("test", 500, 500, 100, 30);
    EXPECT_FALSE(AnchorFieldExtractor::IsInRegion(box, {0, 0, 200, 200}));
}

// ============================================================================
// ParagraphExtractor
// ============================================================================

TEST(ParagraphExtractorTest, ExtractBetween) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("检查所见：", 100, 500, 200, 30),
        MakeBox("肝脏形态正常。", 100, 600, 300, 30),
        MakeBox("胆囊未见异常。", 100, 680, 300, 30),
        MakeBox("诊断意见：", 100, 800, 200, 30),
        MakeBox("未见明显异常。", 100, 880, 300, 30),
    };

    FieldRule rule;
    rule.start_anchors = {"检查所见"};
    rule.end_anchors = {"诊断意见"};

    ExtractedField field;
    bool ok = ParagraphExtractor::Extract(boxes, rule, field);
    EXPECT_TRUE(ok);
    EXPECT_NE(field.value.find("肝脏形态正常"), std::string::npos);
    EXPECT_NE(field.value.find("胆囊未见异常"), std::string::npos);
    EXPECT_EQ(field.value.find("未见明显异常"), std::string::npos);  // After end.
    EXPECT_EQ(field.extraction_method, ExtractionMethod::ParagraphBetweenAnchors);
}

TEST(ParagraphExtractorTest, ExtractBetween_NoEndAnchor) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("检查所见：", 100, 500, 200, 30),
        MakeBox("正常。", 100, 600, 300, 30),
    };

    FieldRule rule;
    rule.start_anchors = {"检查所见"};
    rule.end_anchors = {"诊断意见"};

    ExtractedField field;
    bool ok = ParagraphExtractor::Extract(boxes, rule, field);
    EXPECT_FALSE(ok);
}

TEST(ParagraphExtractorTest, ExtractBetween_NoStartAnchor) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("诊断意见：", 100, 800, 200, 30),
    };

    FieldRule rule;
    rule.start_anchors = {"检查所见"};

    ExtractedField field;
    bool ok = ParagraphExtractor::Extract(boxes, rule, field);
    EXPECT_FALSE(ok);
}

// ============================================================================
// FieldExtractor — Integration
// ============================================================================

TEST(FieldExtractorTest, ExtractAll_WithTemplate) {
    // Build a minimal OCR result (normalized coords 0–1000).
    std::vector<OcrTextBox> boxes = {
        MakeBox("姓名：", 100, 50, 120, 30),
        MakeBox("张三", 250, 50, 80, 30),
        MakeBox("性别：", 500, 50, 120, 30),
        MakeBox("男", 650, 50, 60, 30),
        MakeBox("年龄：", 100, 100, 120, 30),
        MakeBox("37岁", 250, 100, 60, 30),
        MakeBox("出生日期：", 100, 150, 180, 30),
        MakeBox("1988-09-21", 300, 150, 150, 30),
        MakeBox("检查所见：", 100, 400, 200, 30),
        MakeBox("腹部未见异常。", 100, 480, 400, 30),
        MakeBox("诊断意见：", 100, 600, 200, 30),
        MakeBox("定期复查。", 100, 680, 200, 30),
    };

    // Build a minimal template.
    ReportTemplate tmpl;
    tmpl.template_id = "test_v1";
    tmpl.document_type = "ultrasound_report";

    FieldRule name_rule;
    name_rule.field_name = "name";
    name_rule.anchors = {"姓名"};
    name_rule.search_direction = "right";
    name_rule.same_line_tolerance = 20;
    name_rule.max_distance = 250;
    name_rule.data_type = "string";
    name_rule.extraction_type = "anchor_right";
    tmpl.fields.push_back(name_rule);

    FieldRule gender_rule;
    gender_rule.field_name = "gender";
    gender_rule.anchors = {"性别"};
    gender_rule.search_direction = "right";
    gender_rule.same_line_tolerance = 20;
    gender_rule.max_distance = 200;
    gender_rule.data_type = "gender";
    gender_rule.extraction_type = "anchor_right";
    tmpl.fields.push_back(gender_rule);

    FieldRule age_rule;
    age_rule.field_name = "age";
    age_rule.anchors = {"年龄"};
    age_rule.search_direction = "right";
    age_rule.same_line_tolerance = 20;
    age_rule.max_distance = 200;
    age_rule.data_type = "age";
    age_rule.extraction_type = "anchor_right";
    tmpl.fields.push_back(age_rule);

    FieldRule birth_rule;
    birth_rule.field_name = "birth_date";
    birth_rule.anchors = {"出生日期"};
    birth_rule.search_direction = "right";
    birth_rule.same_line_tolerance = 20;
    birth_rule.max_distance = 250;
    birth_rule.data_type = "date";
    birth_rule.extraction_type = "anchor_right";
    tmpl.fields.push_back(birth_rule);

    FieldRule findings_rule;
    findings_rule.field_name = "findings";
    findings_rule.start_anchors = {"检查所见"};
    findings_rule.end_anchors = {"诊断意见"};
    findings_rule.extraction_type = "paragraph_between_anchors";
    findings_rule.data_type = "text";
    tmpl.fields.push_back(findings_rule);

    PatientInfo patient;
    ExaminationInfo exam;
    std::vector<std::string> warnings;

    FieldExtractor::ExtractAll(boxes, tmpl, patient, exam, warnings);

    // Verify patient info.
    EXPECT_EQ(patient.name.value, "张三");
    EXPECT_EQ(patient.gender.value, "男");
    EXPECT_EQ(patient.age.value, "37");
    EXPECT_EQ(patient.birth_date.value, "1988-09-21");

    // Verify examination info.
    EXPECT_NE(exam.findings.value.find("腹部未见异常"), std::string::npos);
}

TEST(FieldExtractorTest, ExtractAll_NoTemplate) {
    std::vector<OcrTextBox> boxes = {
        MakeBox("Random text", 100, 100, 200, 30),
    };

    ReportTemplate empty_tmpl;  // No fields.
    PatientInfo patient;
    ExaminationInfo exam;
    std::vector<std::string> warnings;

    FieldExtractor::ExtractAll(boxes, empty_tmpl, patient, exam, warnings);
    // Should not crash; should produce empty results.
    EXPECT_TRUE(patient.name.value.empty());
}

}  // namespace
}  // namespace medical_ocr
