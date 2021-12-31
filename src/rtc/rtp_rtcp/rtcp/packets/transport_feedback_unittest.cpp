#include "rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {
namespace {

using ::testing::ElementsAreArray;

static const int kHeaderSize = 20;
static const int kStatusChunkSize = 2;
static const int kSmallDeltaSize = 1;
static const int kLargeDeltaSize = 2;

static const int64_t kDeltaLimitUs = 0xFF * TransportFeedback::kDeltaScaleFactor;

class T(FeedbackTester) {
public:
    T(FeedbackTester)() : T(FeedbackTester)(true) {}
    explicit T(FeedbackTester)(bool include_timestamps)
        : expected_size_(kAnySize),
          default_delta_us_(TransportFeedback::kDeltaScaleFactor * 4),
          include_timestamps_(include_timestamps) {}

    void WithExpectedSize(size_t expected_size) {
        expected_size_ = expected_size;
    }

    void WithDefaultDelta(int64_t delta) { default_delta_us_ = delta; }

    void WithInput(const uint16_t received_seq[],
                   const int64_t received_ts[],
                   uint16_t length) {
        std::unique_ptr<int64_t[]> temp_timestamps;
        if (received_ts == nullptr) {
            temp_timestamps.reset(new int64_t[length]);
            GenerateReceiveTimestamps(received_seq, length, temp_timestamps.get());
            received_ts = temp_timestamps.get();
        }

        expected_seq_.clear();
        expected_deltas_.clear();
        feedback_.reset(new TransportFeedback(include_timestamps_));
        feedback_->SetBase(received_seq[0], received_ts[0]);
        ASSERT_TRUE(feedback_->IsConsistent());

        int64_t last_time = feedback_->GetBaseTimeUs();
        for (int i = 0; i < length; ++i) {
            int64_t time = received_ts[i];
            EXPECT_TRUE(feedback_->AddReceivedPacket(received_seq[i], time));

            if (last_time != -1) {
                int64_t delta = time - last_time;
                expected_deltas_.push_back(delta);
            }
            last_time = time;
        }
        ASSERT_TRUE(feedback_->IsConsistent());
        expected_seq_.insert(expected_seq_.begin(), &received_seq[0],
                            &received_seq[length]);
    }

    void VerifyPacket() {
        ASSERT_TRUE(feedback_->IsConsistent());
        serialized_ = feedback_->Build();
        VerifyInternal();
        feedback_ = TransportFeedback::ParseFrom(serialized_.data(), serialized_.size());
        ASSERT_NE(nullptr, feedback_);
        ASSERT_TRUE(feedback_->IsConsistent());
        EXPECT_EQ(include_timestamps_, feedback_->IncludeTimestamps());
        VerifyInternal();
    }

    static const size_t kAnySize = static_cast<size_t>(0) - 1;

private:
    void VerifyInternal() {
        if (expected_size_ != kAnySize) {
            // Round up to whole 32-bit words.
            size_t expected_size_words = (expected_size_ + 3) / 4;
            size_t expected_size_bytes = expected_size_words * 4;
            EXPECT_EQ(expected_size_bytes, serialized_.size());
        }

        std::vector<uint16_t> actual_seq_nums;
        std::vector<int64_t> actual_deltas_us;
        for (const auto& packet : feedback_->GetReceivedPackets()) {
            actual_seq_nums.push_back(packet.sequence_number());
            actual_deltas_us.push_back(packet.delta_us());
        }
        EXPECT_THAT(actual_seq_nums, ElementsAreArray(expected_seq_));
        if (include_timestamps_) {
            EXPECT_THAT(actual_deltas_us, ElementsAreArray(expected_deltas_));
        }
    }

    void GenerateReceiveTimestamps(const uint16_t seq[],
                                    const size_t length,
                                    int64_t* timestamps) {
        uint16_t last_seq = seq[0];
        int64_t offset = 0;

        for (size_t i = 0; i < length; ++i) {
            if (seq[i] < last_seq)
                offset += 0x10000 * default_delta_us_;
            last_seq = seq[i];

            timestamps[i] = offset + (last_seq * default_delta_us_);
        }
    }

private:
    std::vector<uint16_t> expected_seq_;
    std::vector<int64_t> expected_deltas_;
    size_t expected_size_;
    int64_t default_delta_us_;
    std::unique_ptr<TransportFeedback> feedback_;
    CopyOnWriteBuffer serialized_;
    bool include_timestamps_;
};
    
} // namespace


} // namespace test
} // namespace naivertc
