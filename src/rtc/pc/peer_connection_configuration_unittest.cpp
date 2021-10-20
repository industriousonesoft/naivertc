#include "rtc/pc/peer_connection_configuration.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(PC_IceServerTest, CreateFromStunURL) {
    const std::string url = "stun:stun.l.google.com:19302";
    IceServer ice_server(url);

    EXPECT_EQ(ice_server.hostname(), "stun.l.google.com");
    EXPECT_EQ(ice_server.port(), 19302);
    EXPECT_EQ(ice_server.type(), IceServer::Type::STUN);
}

TEST(PC_IceServerTest, CreateFromTurnURL) {
    const std::string url = "turn:192.158.29.39:3478?transport=udp";
    IceServer ice_server(url);

    EXPECT_EQ(ice_server.hostname(), "192.158.29.39");
    EXPECT_EQ(ice_server.port(), 3478);
    EXPECT_EQ(ice_server.type(), IceServer::Type::TURN);
    EXPECT_EQ(ice_server.relay_type(), IceServer::RelayType::TURN_UDP);
}

}
}