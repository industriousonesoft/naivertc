#include "rtc/transports/ice_transport.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(SDP_IceTransportDescriptionTest, Constructor) {
    IceTransport::Description sdp(sdp::Type::OFFER, sdp::Role::ACT_PASS, "5gAx", "UaOtA7vsDocYINrXSTPWph");

    EXPECT_EQ(sdp.type(), sdp::Type::OFFER);
    EXPECT_EQ(sdp.role(), sdp::Role::ACT_PASS);
    EXPECT_EQ(sdp.ice_ufrag().has_value(), true);
    EXPECT_EQ(sdp.ice_ufrag().value(), "5gAx");
    EXPECT_EQ(sdp.ice_pwd().has_value(), true);
    EXPECT_EQ(sdp.ice_pwd().value(), "UaOtA7vsDocYINrXSTPWph");
}

TEST(SDP_IceTransportDescriptionTest, GenerateSDP) {
    IceTransport::Description sdp(sdp::Type::OFFER, sdp::Role::ACT_PASS, "5gAx", "UaOtA7vsDocYINrXSTPWph");

    const std::string expected_sdp_string = 
R"(m=application 0 ICE/SDP
c=IN IP4 0.0.0.0
a=ice-ufrag:5gAx
a=ice-pwd:UaOtA7vsDocYINrXSTPWph
)";

    auto sdp_string = sdp.GenerateSDP("\n");

    EXPECT_EQ(sdp_string, expected_sdp_string);

}

TEST(SDP_IceTransportDescriptionTest, ParseSDP) {
    const std::string sdp_string = 
R"(m=application 0 ICE/SDP
c=IN IP4 0.0.0.0
a=ice-ufrag:8uhx
a=ice-pwd:UafidNgHgVsfdWph)";

    auto sdp = IceTransport::Description::Parse(sdp_string, sdp::Type::ANSWER, sdp::Role::PASSIVE);

    EXPECT_EQ(sdp.type(), sdp::Type::ANSWER);
    EXPECT_EQ(sdp.role(), sdp::Role::PASSIVE);
    EXPECT_EQ(sdp.ice_ufrag().has_value(), true);
    EXPECT_EQ(sdp.ice_ufrag().value(), "8uhx");
    EXPECT_EQ(sdp.ice_pwd().has_value(), true);
    EXPECT_EQ(sdp.ice_pwd().value(), "UafidNgHgVsfdWph");

}
    
} // namespace test
} // namespace naivertc n