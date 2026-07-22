#include "gtest/gtest.h"
#include "src/validation/date_validator.h"

namespace medical_ocr {
namespace {

TEST(DateValidatorTest, Normalize_ISO) {
    auto r = DateValidator::Normalize("2026-07-20");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.normalized, "2026-07-20");
}

TEST(DateValidatorTest, Normalize_ChineseFormat) {
    auto r = DateValidator::Normalize("2026年7月20日");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.normalized, "2026-07-20");
}

TEST(DateValidatorTest, Normalize_ChineseFormatSingleDigit) {
    auto r = DateValidator::Normalize("2026年1月5日");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.normalized, "2026-01-05");
}

TEST(DateValidatorTest, Normalize_SlashFormat) {
    auto r = DateValidator::Normalize("2026/07/20");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.normalized, "2026-07-20");
}

TEST(DateValidatorTest, Normalize_DotFormat) {
    auto r = DateValidator::Normalize("2026.07.20");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.normalized, "2026-07-20");
}

TEST(DateValidatorTest, Normalize_EightDigits) {
    auto r = DateValidator::Normalize("20260720");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.normalized, "2026-07-20");
}

TEST(DateValidatorTest, Normalize_InvalidDate) {
    auto r = DateValidator::Normalize("2026-13-01");  // Month 13.
    EXPECT_FALSE(r.valid);
}

TEST(DateValidatorTest, Normalize_InvalidDateFeb30) {
    auto r = DateValidator::Normalize("2026-02-30");
    EXPECT_FALSE(r.valid);
}

TEST(DateValidatorTest, Normalize_Empty) {
    auto r = DateValidator::Normalize("");
    EXPECT_FALSE(r.valid);
}

TEST(DateValidatorTest, Normalize_Garbage) {
    auto r = DateValidator::Normalize("not a date");
    EXPECT_FALSE(r.valid);
}

TEST(DateValidatorTest, IsValidDate_LeapYear) {
    EXPECT_TRUE(DateValidator::IsValidDate(2024, 2, 29));   // Leap year.
    EXPECT_FALSE(DateValidator::IsValidDate(2023, 2, 29));  // Non-leap.
    EXPECT_TRUE(DateValidator::IsValidDate(2000, 2, 29));   // Century leap.
    EXPECT_FALSE(DateValidator::IsValidDate(1900, 2, 29));  // Century non-leap.
}

TEST(DateValidatorTest, IsValidDate_Basic) {
    EXPECT_TRUE(DateValidator::IsValidDate(2026, 1, 1));
    EXPECT_TRUE(DateValidator::IsValidDate(2026, 12, 31));
    EXPECT_FALSE(DateValidator::IsValidDate(2026, 0, 1));
    EXPECT_FALSE(DateValidator::IsValidDate(2026, 13, 1));
    EXPECT_FALSE(DateValidator::IsValidDate(2026, 1, 0));
    EXPECT_FALSE(DateValidator::IsValidDate(2026, 1, 32));
}

TEST(DateValidatorTest, CalculateAge) {
    // Test with fixed reference date.
    int age = DateValidator::CalculateAge("1990-03-07", "2026-07-20");
    EXPECT_EQ(age, 36);  // 2026 - 1990 = 36, birthday passed.

    int age2 = DateValidator::CalculateAge("1990-09-01", "2026-07-20");
    EXPECT_EQ(age2, 35);  // Birthday not yet passed in 2026.
}

}  // namespace
}  // namespace medical_ocr
