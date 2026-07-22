#include "gtest/gtest.h"
#include "src/validation/id_card_validator.h"

namespace medical_ocr {
namespace {

TEST(IdCardValidatorTest, ValidIdNumber) {
    // Verified valid ID: first 17 digits = 11010119900307663, checksum = 2
    auto result = IdCardValidator::Validate("110101199003076632");
    EXPECT_TRUE(result.format_valid);
    EXPECT_TRUE(result.checksum_valid);
    EXPECT_EQ(result.birth_date, "1990-03-07");
    EXPECT_EQ(result.gender, "男");  // digit 17 = 3 (odd)
}

TEST(IdCardValidatorTest, ValidIdNumber_Female) {
    // Verified valid ID: first 17 digits = 11010119900307684, sum=240, 240%11=9, checksum='3'
    auto result = IdCardValidator::Validate("110101199003076843");
    EXPECT_TRUE(result.format_valid);
    EXPECT_TRUE(result.checksum_valid);
    EXPECT_EQ(result.gender, "女");  // digit 17 = 4 (even)
}

TEST(IdCardValidatorTest, InvalidChecksum) {
    // Change the checksum digit.
    auto result = IdCardValidator::Validate("110101199003076631");
    EXPECT_TRUE(result.format_valid);
    EXPECT_FALSE(result.checksum_valid);
}

TEST(IdCardValidatorTest, TooShort) {
    auto result = IdCardValidator::Validate("12345");
    EXPECT_FALSE(result.format_valid);
}

TEST(IdCardValidatorTest, TooLong) {
    auto result = IdCardValidator::Validate("1234567890123456789");
    EXPECT_FALSE(result.format_valid);
}

TEST(IdCardValidatorTest, Empty) {
    auto result = IdCardValidator::Validate("");
    EXPECT_FALSE(result.format_valid);
}

TEST(IdCardValidatorTest, NonDigitCharacters) {
    auto result = IdCardValidator::Validate("1101011990A307663X");
    EXPECT_FALSE(result.format_valid);
}

TEST(IdCardValidatorTest, ComputeChecksum) {
    // Verified test vectors from GB 11643-1999 algorithm.
    // 11010119900307663 → sum=230, 230%11=10 → checksum '2'
    char cs = IdCardValidator::ComputeChecksum("11010119900307663");
    EXPECT_EQ(cs, '2');

    // 11010119900307684 → sum=240, 240%11=9 → checksum '3'
    char cs2 = IdCardValidator::ComputeChecksum("11010119900307684");
    EXPECT_EQ(cs2, '3');
}

TEST(IdCardValidatorTest, ComputeChecksum_WrongLength) {
    EXPECT_EQ(IdCardValidator::ComputeChecksum("123"), '\0');
    EXPECT_EQ(IdCardValidator::ComputeChecksum(""), '\0');
}

TEST(IdCardValidatorTest, OcrCorrection_LetterToDigit) {
    std::string corrected = IdCardValidator::CorrectOcrErrors("44O3O5l988O92lZ234");
    EXPECT_EQ(corrected, "440305198809212234");
}

TEST(IdCardValidatorTest, OcrCorrection_BToEight) {
    std::string corrected = IdCardValidator::CorrectOcrErrors("44O3O5l988O92lB234");
    EXPECT_EQ(corrected, "440305198809218234");
}

TEST(IdCardValidatorTest, OcrCorrection_XAtEnd) {
    // The last character can be X.
    std::string corrected = IdCardValidator::CorrectOcrErrors(
        "11010119900307663x");
    EXPECT_EQ(corrected.back(), 'X');
}

TEST(IdCardValidatorTest, ExtractBirthDate_Valid) {
    std::string bd = IdCardValidator::ExtractBirthDate("440305198809211234");
    EXPECT_EQ(bd, "1988-09-21");
}

TEST(IdCardValidatorTest, ExtractBirthDate_Short) {
    EXPECT_TRUE(IdCardValidator::ExtractBirthDate("123").empty());
}

TEST(IdCardValidatorTest, ExtractGender_Male) {
    EXPECT_EQ(IdCardValidator::ExtractGender("440305198809211234"), "男");
}

TEST(IdCardValidatorTest, ExtractGender_Female) {
    EXPECT_EQ(IdCardValidator::ExtractGender("440305198809212345"), "女");
}

}  // namespace
}  // namespace medical_ocr
