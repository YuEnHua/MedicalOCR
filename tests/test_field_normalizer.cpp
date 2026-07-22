#include "gtest/gtest.h"
#include "src/validation/field_normalizer.h"

namespace medical_ocr {
namespace {

TEST(FieldNormalizerTest, Gender_ChineseMale) {
    EXPECT_EQ(FieldNormalizer::NormalizeGender("男"), "男");
}

TEST(FieldNormalizerTest, Gender_ChineseFemale) {
    EXPECT_EQ(FieldNormalizer::NormalizeGender("女"), "女");
}

TEST(FieldNormalizerTest, Gender_EnglishMale) {
    EXPECT_EQ(FieldNormalizer::NormalizeGender("Male"), "男");
    EXPECT_EQ(FieldNormalizer::NormalizeGender("male"), "男");
    EXPECT_EQ(FieldNormalizer::NormalizeGender("M"), "男");
}

TEST(FieldNormalizerTest, Gender_EnglishFemale) {
    EXPECT_EQ(FieldNormalizer::NormalizeGender("Female"), "女");
    EXPECT_EQ(FieldNormalizer::NormalizeGender("female"), "女");
    EXPECT_EQ(FieldNormalizer::NormalizeGender("F"), "女");
}

TEST(FieldNormalizerTest, Gender_Numeric) {
    EXPECT_EQ(FieldNormalizer::NormalizeGender("1"), "男");
    EXPECT_EQ(FieldNormalizer::NormalizeGender("0"), "女");
    EXPECT_EQ(FieldNormalizer::NormalizeGender("2"), "女");
}

TEST(FieldNormalizerTest, Gender_Empty) {
    EXPECT_TRUE(FieldNormalizer::NormalizeGender("").empty());
}

TEST(FieldNormalizerTest, Gender_Unrecognized) {
    EXPECT_TRUE(FieldNormalizer::NormalizeGender("未知").empty());
}

TEST(FieldNormalizerTest, Age_WithSuffix) {
    EXPECT_EQ(FieldNormalizer::ExtractAge("37岁"), 37);
    EXPECT_EQ(FieldNormalizer::ExtractAge("37Y"), 37);
    EXPECT_EQ(FieldNormalizer::ExtractAge("37y"), 37);
}

TEST(FieldNormalizerTest, Age_PlainNumber) {
    EXPECT_EQ(FieldNormalizer::ExtractAge("37"), 37);
    EXPECT_EQ(FieldNormalizer::ExtractAge("0"), 0);
    EXPECT_EQ(FieldNormalizer::ExtractAge("150"), 150);
}

TEST(FieldNormalizerTest, Age_WithWhitespace) {
    EXPECT_EQ(FieldNormalizer::ExtractAge("  37 岁  "), 37);
}

TEST(FieldNormalizerTest, Age_OutOfRange) {
    EXPECT_EQ(FieldNormalizer::ExtractAge("151"), -1);
    EXPECT_EQ(FieldNormalizer::ExtractAge("999"), -1);
}

TEST(FieldNormalizerTest, Age_NotANumber) {
    EXPECT_EQ(FieldNormalizer::ExtractAge("abc"), -1);
    EXPECT_EQ(FieldNormalizer::ExtractAge(""), -1);
}

TEST(FieldNormalizerTest, Strip_Whitespace) {
    EXPECT_EQ(FieldNormalizer::Strip("  hello  "), "hello");
}

TEST(FieldNormalizerTest, Strip_TrailingColon) {
    EXPECT_EQ(FieldNormalizer::Strip("姓名："), "姓名");
    EXPECT_EQ(FieldNormalizer::Strip("Name:"), "Name");
}

TEST(FieldNormalizerTest, IsReasonableAge) {
    EXPECT_TRUE(FieldNormalizer::IsReasonableAge(0));
    EXPECT_TRUE(FieldNormalizer::IsReasonableAge(50));
    EXPECT_TRUE(FieldNormalizer::IsReasonableAge(150));
    EXPECT_FALSE(FieldNormalizer::IsReasonableAge(-1));
    EXPECT_FALSE(FieldNormalizer::IsReasonableAge(151));
}

}  // namespace
}  // namespace medical_ocr
