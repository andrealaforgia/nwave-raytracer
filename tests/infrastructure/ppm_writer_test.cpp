#include <gtest/gtest.h>
#include "infrastructure/ppm_writer.h"
#include "core/vec3.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>

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

    std::string read_file() {
        std::ifstream file(test_file);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
};

TEST_F(PpmWriterTest, FileStartsWithP3Header) {
    write_ppm(test_file, pixels, width, height);
    std::string content = read_file();

    EXPECT_EQ(content.substr(0, 2), "P3");
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

    std::ifstream file(test_file);
    std::string line;
    std::getline(file, line); // P3
    std::getline(file, line); // dimensions
    std::getline(file, line); // 255

    int value_count = 0;
    int r, g, b;
    while (file >> r >> g >> b) {
        EXPECT_GE(r, 0);
        EXPECT_LE(r, 255);
        EXPECT_GE(g, 0);
        EXPECT_LE(g, 255);
        EXPECT_GE(b, 0);
        EXPECT_LE(b, 255);
        value_count++;
    }

    EXPECT_EQ(value_count, width * height);
}

TEST_F(PpmWriterTest, PixelValuesAreCorrectlyConverted) {
    write_ppm(test_file, pixels, width, height);

    std::ifstream file(test_file);
    std::string line;
    std::getline(file, line); // P3
    std::getline(file, line); // dimensions
    std::getline(file, line); // 255

    int r, g, b;
    // First pixel: Color3(1.0, 0.0, 0.0) -> 255 0 0
    file >> r >> g >> b;
    EXPECT_EQ(r, 255);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 0);

    // Second pixel: Color3(0.0, 1.0, 0.0) -> 0 255 0
    file >> r >> g >> b;
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g, 255);
    EXPECT_EQ(b, 0);
}
