#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(filter) = "-*Base_*:*Common_*:*Media_*:*H264_*:*PC_*:*RTP_RTCP_*:*FEC_*:*SDP_*:*VCM_*";
    return RUN_ALL_TESTS();
}