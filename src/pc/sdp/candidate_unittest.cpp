#include "pc/sdp/candidate.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace testing;

namespace naivertc {
namespace test {

TEST(CandidateTest, CreateFromSDPLine) {
    const std::string sdp = "a=candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321";

    naivertc::Candidate candidate(sdp);

    EXPECT_EQ(candidate.foundation(), "1");
    EXPECT_EQ(candidate.component_id(), 1);
    EXPECT_EQ(candidate.transport_type(), Candidate::TransportType::UDP);
    EXPECT_EQ(candidate.priority(), 9654321);
    EXPECT_EQ(candidate.host_name(), "212.223.223.223");
    EXPECT_EQ(candidate.service(), "12345");
    EXPECT_EQ(candidate.type(), Candidate::Type::SERVER_REFLEXIVE);
    EXPECT_EQ(candidate.isResolved(), false);

}

TEST(CandidateTest, BuildFromCandidateSDP) {
    const std::string sdp = "candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321";

    naivertc::Candidate candidate(sdp);

    EXPECT_EQ(candidate.foundation(), "1");
    EXPECT_EQ(candidate.component_id(), 1);
    EXPECT_EQ(candidate.transport_type(), Candidate::TransportType::UDP);
    EXPECT_EQ(candidate.priority(), 9654321);
    EXPECT_EQ(candidate.host_name(), "212.223.223.223");
    EXPECT_EQ(candidate.service(), "12345");
    EXPECT_EQ(candidate.type(), Candidate::Type::SERVER_REFLEXIVE);
    EXPECT_EQ(candidate.isResolved(), false);
}

TEST(CandidateTest, ToString) {
    const std::string sdp = "candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321";

    naivertc::Candidate candidate(sdp);

    std::string str = std::string(candidate);

    EXPECT_EQ(str, sdp);
}

TEST(CandidateTest, BuildSDPLine) {
    const std::string sdp = "a=candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321";

    naivertc::Candidate candidate(sdp);

    std::string build_sdp = candidate.sdp_line();

    EXPECT_EQ(build_sdp, sdp);
}

}
}