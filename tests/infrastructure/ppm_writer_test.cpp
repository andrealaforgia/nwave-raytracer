#include <gtest/gtest.h>
#include "infrastructure/ppm_writer.h"
#include "core/vec3.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

using namespace nwave;

class PpmWriterTest : public ::testing::Test {
protected:
    std::string test_file = "test_output.ppm";
    int width = 3;
    int height = 2;
    std::vector<Color3> pixels;

    void SetUp() override {
        pixels = {
            Color3(1.0, 0.0, 0.0), Color3(0.0, 1.0, 0.0), Color3(0.0, 0.0, 1.0),
            Color3(0.5, 0.5, 0.5), Color3(0.0, 0.0, 0.0), Color3(1.0, 1.0, 1.0),
        };
    }

    void TearDown() override {
        std::remove(test_file.c_str());
    }

    std::string read_file_text() {
        std::ifstream file(test_file);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    std::vector<uint8_t> read_file_binary() {
        std::ifstream file(test_file, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
    }

    // Returns the byte offset where pixel data starts (after "P6\n<w> <h>\n255\n")
    size_t find_pixel_data_offset(const std::vector<uint8_t>& data) {
        int newline_count = 0;
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i] == '\n') {
                newline_count++;
                if (newline_count == 3) return i + 1;
            }
        }
        return data.size();
    }
};

TEST_F(PpmWriterTest, FileStartsWithP6Header) {
    write_ppm(test_file, pixels, width, height);
    std::string content = read_file_text();

    EXPECT_EQ(content.substr(0, 2), "P6");
}

TEST_F(PpmWriterTest, SecondLineHasCorrectDimensions) {
    write_ppm(test_file, pixels, width, height);

    std::ifstream file(test_file);
    std::string line1, line2;
    std::getline(file, line1);
    std::getline(file, line2);

    EXPECT_EQ(line2, "3 2");
}

TEST_F(PpmWriterTest, ThirdLineHasMaxColorValue255) {
    write_ppm(test_file, pixels, width, height);

    std::ifstream file(test_file);
    std::string line1, line2, line3;
    std::getline(file, line1);
    std::getline(file, line2);
    std::getline(file, line3);

    EXPECT_EQ(line3, "255");
}

TEST_F(PpmWriterTest, AllRgbValuesAreInValidRange) {
    write_ppm(test_file, pixels, width, height);

    auto data = read_file_binary();
    size_t offset = find_pixel_data_offset(data);
    size_t pixel_bytes = data.size() - offset;

    // P6 binary: 3 bytes per pixel (R, G, B)
    EXPECT_EQ(pixel_bytes, static_cast<size_t>(width * height * 3));

    // All byte values are inherently 0-255, so just verify count
    for (size_t i = offset; i < data.size(); i++) {
        EXPECT_GE(data[i], 0u);
        EXPECT_LE(data[i], 255u);
    }
}

TEST_F(PpmWriterTest, PixelValuesAreCorrectlyConverted) {
    write_ppm(test_file, pixels, width, height);

    auto data = read_file_binary();
    size_t offset = find_pixel_data_offset(data);

    ASSERT_GE(data.size(), offset + 6u); // At least 2 pixels worth

    // First pixel: Color3(1.0, 0.0, 0.0) -> 255 0 0
    EXPECT_EQ(data[offset + 0], 255u);
    EXPECT_EQ(data[offset + 1], 0u);
    EXPECT_EQ(data[offset + 2], 0u);

    // Second pixel: Color3(0.0, 1.0, 0.0) -> 0 255 0
    EXPECT_EQ(data[offset + 3], 0u);
    EXPECT_EQ(data[offset + 4], 255u);
    EXPECT_EQ(data[offset + 5], 0u);
}
