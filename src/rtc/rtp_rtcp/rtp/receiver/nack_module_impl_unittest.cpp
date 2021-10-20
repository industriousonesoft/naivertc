#include "rtc/rtp_rtcp/rtp/receiver/nack_module_impl.hpp"
#include "rtc/base/time/clock_simulated.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

class RTP_RTCP_NackModuleTest : public ::testing::Test {
protected:
    RTP_RTCP_NackModuleTest() 
        : clock_(std::make_shared<SimulatedClock>(0)) {}

    void CreateNackModule(int64_t send_nack_delay_ms = 0) {
        nack_module_ = std::make_unique<NackModuleImpl>(clock_, send_nack_delay_ms);
        nack_module_->UpdateRtt(kDefaultRttMs);
    }

    size_t InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered) {
        auto ret = nack_module_->InsertPacket(seq_num, is_keyframe, is_recovered);
        sent_nacks_.insert(sent_nacks_.end(), ret.nack_list_to_send.begin(), ret.nack_list_to_send.end());
        if (ret.keyframe_requested) {
            keyframes_requested_++;
        }
        return ret.nacks_sent_for_seq_num;
    }

    void UpdateRtt(int64_t rtt_ms) {
        nack_module_->UpdateRtt(rtt_ms);
    }

    void PeriodicUpdate() {
        auto sent_nacks = nack_module_->NackListOnRttPassed();
        sent_nacks_.insert(sent_nacks_.end(), sent_nacks.begin(), sent_nacks.end());
    }

    static constexpr int64_t kDefaultRttMs = 20;
    std::shared_ptr<SimulatedClock> clock_;
    std::vector<uint16_t> sent_nacks_;
    size_t keyframes_requested_ = 0;
    std::unique_ptr<NackModuleImpl> nack_module_;
};

TEST_F(RTP_RTCP_NackModuleTest, NackOnePacket) {
    CreateNackModule();
    sent_nacks_.clear();
    InsertPacket(1, false, false);
    InsertPacket(3, false, false);
    ASSERT_EQ(1u, sent_nacks_.size());
    EXPECT_EQ(2, sent_nacks_[0]);
}

TEST_F(RTP_RTCP_NackModuleTest, WrappingSeqNumClearToKeyframe) {
    // Filtered by sequence number
    CreateNackModule();
    sent_nacks_.clear();
    InsertPacket(0xfffe, false, false);
    InsertPacket(1, false, false);
    ASSERT_EQ(2u, sent_nacks_.size());
    EXPECT_EQ(0xffff, sent_nacks_[0]);
    EXPECT_EQ(0, sent_nacks_[1]);

    sent_nacks_.clear();
    InsertPacket(2, true, false);
    ASSERT_EQ(0u, sent_nacks_.size());

    sent_nacks_.clear();
    InsertPacket(501, true, false);
    ASSERT_EQ(498u, sent_nacks_.size());
    for (uint16_t seq_num = 3; seq_num < 501; ++seq_num) {
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 3]);
    }

    sent_nacks_.clear();
    InsertPacket(1001, false, false);
    EXPECT_EQ(499u, sent_nacks_.size());
    for (uint16_t seq_num = 502; seq_num < 1001; ++seq_num)
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 502]);

    sent_nacks_.clear();
    // Filtered by timing
    clock_->AdvanceTimeMilliseconds(100);
    // Call NackListOnRttPassed() explicitly to simulate repeating task.
    PeriodicUpdate();
    ASSERT_EQ(999u, sent_nacks_.size());
    EXPECT_EQ(0xffff, sent_nacks_[0]);
    EXPECT_EQ(0, sent_nacks_[1]);
    for (int seq_num = 3; seq_num < 501; ++seq_num)
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 1]);
    for (int seq_num = 502; seq_num < 1001; ++seq_num)
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 2]);

    // Adding packet 1004 will cause the nack list to reach it's max limit.
    // It will then clear all nacks up to the next keyframe (seq num 2),
    // thus removing 0xffff and 0 from the nack list.
    sent_nacks_.clear();
    InsertPacket(1004, false, false);
    ASSERT_EQ(2u, sent_nacks_.size());
    EXPECT_EQ(1002, sent_nacks_[0]);
    EXPECT_EQ(1003, sent_nacks_[1]);

    sent_nacks_.clear();
    clock_->AdvanceTimeMilliseconds(100);
    PeriodicUpdate();
    ASSERT_EQ(999u, sent_nacks_.size());
    for (int seq_num = 3; seq_num < 501; ++seq_num)
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 3]);
    for (int seq_num = 502; seq_num < 1001; ++seq_num)
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 4]);

    // Adding packet 1007 will cause the nack module to overflow again, thus
    // clearing everything up to 501 which is the next keyframe.
    sent_nacks_.clear();
    InsertPacket(1007, false, false);
    ASSERT_EQ(2u, sent_nacks_.size());
    EXPECT_EQ(1005, sent_nacks_[0]);
    EXPECT_EQ(1006, sent_nacks_[1]);

    sent_nacks_.clear();
    clock_->AdvanceTimeMilliseconds(100);
    PeriodicUpdate();
    ASSERT_EQ(503u, sent_nacks_.size());
    for (int seq_num = 502; seq_num < 1001; ++seq_num)
        EXPECT_EQ(seq_num, sent_nacks_[seq_num - 502]);
    EXPECT_EQ(1002, sent_nacks_[499]);
    EXPECT_EQ(1003, sent_nacks_[500]);
    EXPECT_EQ(1005, sent_nacks_[501]);
    EXPECT_EQ(1006, sent_nacks_[502]);
}

TEST_F(RTP_RTCP_NackModuleTest, ResendNack) {
    CreateNackModule();
    sent_nacks_.clear();
    InsertPacket(1, false, false);
    InsertPacket(3, false, false);
    size_t expected_nacks_sent = 1;
    ASSERT_EQ(expected_nacks_sent, sent_nacks_.size());
    EXPECT_EQ(2u, sent_nacks_[0]);

    UpdateRtt(1);
    clock_->AdvanceTimeMilliseconds(1);
    PeriodicUpdate();
    EXPECT_EQ(++expected_nacks_sent, sent_nacks_.size());

    for (int i = 2; i < 10; ++i) {
        // Change RTT, above the 40ms max for exponential backoff.
        TimeDelta rtt = TimeDelta::Millis(160);  // + (i * 10 - 40)
        UpdateRtt(rtt.ms());

        // Move to one millisecond before next allowed NACK.
        clock_->AdvanceTimeMilliseconds(rtt.ms() - 1);
        PeriodicUpdate();
        EXPECT_EQ(expected_nacks_sent, sent_nacks_.size());

        // Move to one millisecond after next allowed NACK.
        // After rather than on to avoid rounding errors.
        clock_->AdvanceTimeMilliseconds(2);
        PeriodicUpdate();
        EXPECT_EQ(++expected_nacks_sent, sent_nacks_.size());
    }

    // Giving up after 10 tries. so not try to resend
    clock_->AdvanceTimeMilliseconds(3000);
    PeriodicUpdate();
    EXPECT_EQ(expected_nacks_sent, sent_nacks_.size());
}

TEST_F(RTP_RTCP_NackModuleTest, ResendPacketMaxRetries) {
    CreateNackModule();
    sent_nacks_.clear();
    InsertPacket(1, false, false);
    InsertPacket(3, false, false);
    ASSERT_EQ(1u, sent_nacks_.size());
    EXPECT_EQ(2, sent_nacks_[0]);

    int backoff_factor = 1;
    for (size_t retries = 1; retries < 10; ++retries) {
        // Exponential backoff, so that we don't reject NACK because of time.
        clock_->AdvanceTimeMilliseconds(backoff_factor * kDefaultRttMs);
        backoff_factor *= 2;
        PeriodicUpdate();
        EXPECT_EQ(retries + 1, sent_nacks_.size());
    }

    clock_->AdvanceTimeMilliseconds(backoff_factor * kDefaultRttMs);
    PeriodicUpdate();
    EXPECT_EQ(10u, sent_nacks_.size());
}

TEST_F(RTP_RTCP_NackModuleTest, TooLargeNackList) {
    CreateNackModule();
    InsertPacket(0, false, false);
    InsertPacket(1001, false, false);
    EXPECT_EQ(1000u, sent_nacks_.size());
    EXPECT_EQ(0, keyframes_requested_);
    InsertPacket(1003, false, false);
    EXPECT_EQ(1000u, sent_nacks_.size());
    EXPECT_EQ(1, keyframes_requested_);
    InsertPacket(1004, false, false);
    EXPECT_EQ(1000u, sent_nacks_.size());
    EXPECT_EQ(1, keyframes_requested_);
}

TEST_F(RTP_RTCP_NackModuleTest, TooLargeNackListWithKeyFrame) {
    CreateNackModule();
    InsertPacket(0, false, false);
    InsertPacket(1, true, false);
    InsertPacket(1001, false, false);
    EXPECT_EQ(999u, sent_nacks_.size());
    EXPECT_EQ(0, keyframes_requested_);
    InsertPacket(1003, false, false);
    EXPECT_EQ(1000u, sent_nacks_.size());
    EXPECT_EQ(0, keyframes_requested_);
    InsertPacket(1005, false, false);
    EXPECT_EQ(1000u, sent_nacks_.size());
    EXPECT_EQ(1, keyframes_requested_);
}

TEST_F(RTP_RTCP_NackModuleTest, ClearUpTo) {
    CreateNackModule();
    InsertPacket(0, false, false);
    InsertPacket(100, false, false);
    EXPECT_EQ(99u, sent_nacks_.size());

    sent_nacks_.clear();
    clock_->AdvanceTimeMilliseconds(100);
    nack_module_->ClearUpTo(50);
    PeriodicUpdate();
    ASSERT_EQ(50u, sent_nacks_.size());
    EXPECT_EQ(50, sent_nacks_[0]);
}

TEST_F(RTP_RTCP_NackModuleTest, ClearUpToWrap) {
    CreateNackModule();
    sent_nacks_.clear();
    InsertPacket(0xfff0, false, false);
    InsertPacket(0xf, false, false);
    EXPECT_EQ(30u, sent_nacks_.size());

    sent_nacks_.clear();
    clock_->AdvanceTimeMilliseconds(100);
    nack_module_->ClearUpTo(0);
    PeriodicUpdate();
    ASSERT_EQ(15u, sent_nacks_.size());
    EXPECT_EQ(0, sent_nacks_[0]);
}

TEST_F(RTP_RTCP_NackModuleTest, PacketNackCount) {
    CreateNackModule();
    EXPECT_EQ(0, InsertPacket(0, false, false));
    EXPECT_EQ(0, InsertPacket(2, false, false));
    EXPECT_EQ(1, InsertPacket(1, false, false));

    sent_nacks_.clear();
    nack_module_->UpdateRtt(100);
    EXPECT_EQ(0, InsertPacket(5, false, false));
    clock_->AdvanceTimeMilliseconds(100);
    PeriodicUpdate();
    EXPECT_EQ(4u, sent_nacks_.size());

    clock_->AdvanceTimeMilliseconds(125);
    PeriodicUpdate();

    EXPECT_EQ(6u, sent_nacks_.size());

    EXPECT_EQ(3, InsertPacket(3, false, false));
    EXPECT_EQ(3, InsertPacket(4, false, false));
    EXPECT_EQ(0, InsertPacket(4, false, false));
}

TEST_F(RTP_RTCP_NackModuleTest, NackListFullAndNoOverlapWithKeyframes) {
    CreateNackModule();
    sent_nacks_.clear();
    const int kMaxNackPackets = 1000;
    const unsigned int kFirstGap = kMaxNackPackets - 20;
    const unsigned int kSecondGap = 200;
    uint16_t seq_num = 0;
    InsertPacket(seq_num++, true, false);
    seq_num += kFirstGap;
    InsertPacket(seq_num++, true, false);
    EXPECT_EQ(kFirstGap, sent_nacks_.size());
    sent_nacks_.clear();
    seq_num += kSecondGap;
    InsertPacket(seq_num, true, false);
    EXPECT_EQ(kSecondGap, sent_nacks_.size());
}

TEST_F(RTP_RTCP_NackModuleTest, HandleFecRecoveredPacket) {
    CreateNackModule();
    InsertPacket(1, false, false);
    InsertPacket(4, false, true);
    EXPECT_EQ(0u, sent_nacks_.size());
    InsertPacket(5, false, false);
    EXPECT_EQ(2u, sent_nacks_.size());
}

TEST_F(RTP_RTCP_NackModuleTest, SendNackWithoutDelay) {
    CreateNackModule();
    InsertPacket(0, false, false);
    InsertPacket(100, false, false);
    EXPECT_EQ(99u, sent_nacks_.size());
}

TEST_F(RTP_RTCP_NackModuleTest, SendNackWithDelay) {
    CreateNackModule(10);
    InsertPacket(0, false, false);
    InsertPacket(100, false, false);
    EXPECT_EQ(0u, sent_nacks_.size());
    clock_->AdvanceTimeMilliseconds(10);
    InsertPacket(106, false, false);
    EXPECT_EQ(99u, sent_nacks_.size());
    clock_->AdvanceTimeMilliseconds(10);
    InsertPacket(109, false, false);
    EXPECT_EQ(104u, sent_nacks_.size());
}
    
} // namespace test
} // namespace naivertc
