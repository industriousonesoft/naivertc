#include "rtc/sdp/candidate.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

#include <string>

using namespace testing;

namespace naivertc {
namespace test {

MY_TEST(CandidateTest, CreateFromSDPLine) {
    const std::string sdp = "a=candidate:2550170968 1 udp 8265471 45.76.53.21 52823 typ relay raddr 113.246.193.40 rport 37467 generation 0 ufrag CE1b network-id 1 network-cost 10";

    sdp::Candidate candidate(sdp);

    EXPECT_EQ(candidate.foundation(), "2550170968");
    EXPECT_EQ(candidate.component_id(), 1);
    EXPECT_EQ(candidate.transport_type(), sdp::Candidate::TransportType::UDP);
    EXPECT_EQ(candidate.priority(), 8265471);
    EXPECT_EQ(candidate.hostname(), "45.76.53.21");
    EXPECT_EQ(candidate.server_port(), "52823");
    EXPECT_EQ(candidate.type(), sdp::Candidate::Type::RELAYED);
    EXPECT_EQ(candidate.isResolved(), false);

}

MY_TEST(CandidateTest, BuildFromCandidateSDP) {
    const std::string sdp = "candidate:2550170968 1 udp 8265471 45.76.53.21 52823 typ relay raddr 113.246.193.40 rport 37467 generation 0 ufrag CE1b network-id 1 network-cost 10";

    sdp::Candidate candidate(sdp);

    EXPECT_EQ(candidate.foundation(), "2550170968");
    EXPECT_EQ(candidate.component_id(), 1);
    EXPECT_EQ(candidate.transport_type(), sdp::Candidate::TransportType::UDP);
    EXPECT_EQ(candidate.priority(), 8265471);
    EXPECT_EQ(candidate.hostname(), "45.76.53.21");
    EXPECT_EQ(candidate.server_port(), "52823");
    EXPECT_EQ(candidate.type(), sdp::Candidate::Type::RELAYED);
    EXPECT_EQ(candidate.isResolved(), false);
}

MY_TEST(CandidateTest, ToString) {
    const std::string sdp = "candidate:2550170968 1 udp 8265471 45.76.53.21 52823 typ relay raddr 113.246.193.40 rport 37467 generation 0 ufrag CE1b network-id 1 network-cost 10";

    sdp::Candidate candidate(sdp);

    std::string str = std::string(candidate);

    EXPECT_EQ(str, sdp);
}

MY_TEST(CandidateTest, BuildSDPLine) {
    const std::string sdp = "a=candidate:2550170968 1 udp 8265471 45.76.53.21 52823 typ relay raddr 113.246.193.40 rport 37467 generation 0 ufrag CE1b network-id 1 network-cost 10";

    sdp::Candidate candidate(sdp);

    std::string build_sdp = candidate.sdp_line();

    EXPECT_EQ(build_sdp, sdp);
}

} // namespace sdp
} // namespace naivertc