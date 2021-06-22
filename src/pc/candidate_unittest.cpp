#include "pc/candidate.hpp"

#include <gtest/gtest.h>

using namespace testing;

namespace naivertc {

    TEST(CandidateTest, CreateFromSDPLine) {
        const std::string sdp = "a=candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321";

        auto candidate = new naivertc::Candidate(sdp);

        EXPECT_EQ(candidate->Foundation(), "1");
        EXPECT_EQ(candidate->ComponentId(), 1);
        EXPECT_EQ(candidate->GetTransportType(), Candidate::TransportType::UDP);
        EXPECT_EQ(candidate->Priority(), 9654321);
        EXPECT_EQ(candidate->HostName(), "212.223.223.223");
        EXPECT_EQ(candidate->Service(), "12345");
        EXPECT_EQ(candidate->GetType(), Candidate::Type::SERVER_REFLEXIVE);

        delete candidate;
    }

    TEST(CandidateTest, BuildSDPLine) {
        const std::string sdp = "a=candidate:1 1 UDP 9654321 212.223.223.223 12345 typ srflx raddr 10.216.33.9 rport 54321";

        auto candidate = new naivertc::Candidate(sdp);

        std::string build_sdp = candidate->SDPLine();

        EXPECT_EQ(build_sdp, sdp);

        delete candidate;
    }

}