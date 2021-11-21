#include "rtc/media/video/codecs/h264/sprop_parameter_parser.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

#include <vector>

namespace naivertc {
namespace test {

class T(SpropParameterParserTest) : public ::testing::Test {
public:
    h264::SpropParameterParser h264_sprop;
};

MY_TEST_F(SpropParameterParserTest, Base64DecodeSprop) {
    // Example sprop string from https://tools.ietf.org/html/rfc3984 .
    EXPECT_TRUE(h264_sprop.Parse("Z0IACpZTBYmI,aMljiA=="));
    static const std::vector<uint8_t> raw_sps{0x67, 0x42, 0x00, 0x0A, 0x96,
                                              0x53, 0x05, 0x89, 0x88};
    static const std::vector<uint8_t> raw_pps{0x68, 0xC9, 0x63, 0x88};
    EXPECT_EQ(raw_sps, h264_sprop.sps_nalu());
    EXPECT_EQ(raw_pps, h264_sprop.pps_nalu());
}

MY_TEST_F(SpropParameterParserTest, InvalidData) {
    // GTEST_SKIP();
    EXPECT_FALSE(h264_sprop.Parse(","));
    EXPECT_FALSE(h264_sprop.Parse(""));
    EXPECT_FALSE(h264_sprop.Parse(",iA=="));
    EXPECT_FALSE(h264_sprop.Parse("iA==,"));
    EXPECT_TRUE(h264_sprop.Parse("iA==,iA=="));
    EXPECT_FALSE(h264_sprop.Parse("--,--"));
    EXPECT_FALSE(h264_sprop.Parse(",,"));
    EXPECT_FALSE(h264_sprop.Parse("iA=="));
}
    
} // namespace test
} // namespace naivertc