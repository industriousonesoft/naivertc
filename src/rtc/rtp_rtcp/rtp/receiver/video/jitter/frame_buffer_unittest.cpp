#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "testing/simulated_clock.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "common/utils_numeric.hpp"
#include "common/utils_random.hpp"
#include "testing/simulated_time_controller.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

using namespace naivertc::rtp::video;
using ::testing::_;

namespace naivertc {
namespace test {
constexpr int kMaxReferences = 5;
constexpr int kFps1 = 1000;         // 1s
constexpr int kFps10 = kFps1 / 10;  // 100 ms
constexpr int kFps20 = kFps1 / 20;  // 50 ms
constexpr size_t kFrameSize = 10;

// VideoReceiveStatisticsObserverMock
class VideoReceiveStatisticsObserverMock : public VideoReceiveStatisticsObserver {
 public:
  MOCK_METHOD(void,
              OnCompleteFrame,
              (bool is_keyframe,
               size_t size_bytes),
              (override));
  MOCK_METHOD(void, OnDroppedFrames, (uint32_t frames_dropped), (override));
  MOCK_METHOD(void,
              OnFrameBufferTimingsUpdated,
              (int max_decode_ms,
               int current_delay_ms,
               int target_delay_ms,
               int jitter_buffer_ms,
               int min_playout_delay_ms,
               int render_delay_ms),
              (override));
};

// FakeTiming
class FakeTiming : public Timing {
public:
    explicit FakeTiming(std::shared_ptr<Clock> clock) : Timing(std::move(clock)) {}

    int64_t last_ms() const { return last_ms_; }
    uint32_t last_timestamp() const { return last_timestamp_; }

    int64_t RenderTimeMs(uint32_t timestamp, int64_t now_ms) const override {
        if (last_ms_ == -1) {
            last_ms_ = now_ms + kDelayMs;
            last_timestamp_ = timestamp;
        }

        uint32_t diff = MinDiff(timestamp, last_timestamp_);
        if (wrap_around_utils::AheadOf(timestamp, last_timestamp_)) {
            last_ms_ += diff / 90; // timestamp diff time in ms
        }else {
            last_ms_ -= diff / 90;
        }

        last_timestamp_ = timestamp;
        return last_ms_;
    }

    int64_t MaxWaitingTimeBeforeDecode(int64_t render_time_ms, int64_t now_ms) override {
        return render_time_ms - now_ms - kDecodeTimeMs;
    }

    std::pair<TimingInfo, bool> GetTimingInfo() const override {
        return {TimingInfo(), true};
    }

    int GetCurrentJitter() {
        auto [info, success] = Timing::GetTimingInfo();
        return info.jitter_delay_ms;
    } 

private:
    static const int kDelayMs = 50;
    static const int kDecodeTimeMs = kDelayMs / 2; // 25 ms
    mutable uint32_t last_timestamp_ = 0;
    mutable int64_t last_ms_ = -1;
};

// FakeFrameToDecode
class FakeFrameToDecode : public FrameToDecode {
public:
    FakeFrameToDecode(VideoFrameType frame_type,
                      int64_t timestamp_ms,
                      int times_nacked,
                      size_t frame_size) 
        : FrameToDecode(CopyOnWriteBuffer(frame_size),
                        frame_type,
                        VideoCodecType::H264,
                        0, /* seq_num_start */
                        0, /* seq_num_end */
                        timestamp_ms * 90, /* timestamp */
                        0, /* ntp_time_ms */
                        times_nacked, /* times_nacked */
                        0, /* min_received_time_ms */
                        0 /* max_received_time_ms */) {}

};

// FrameBufferTest
class T(FrameBufferTest) : public ::testing::Test {
protected:
    T(FrameBufferTest)() 
        : time_controller_(Timestamp::Millis(0)),
          decode_queue_(time_controller_.CreateTaskQueue()),
          timing_(std::make_shared<FakeTiming>(time_controller_.Clock())),
          stats_observer_(std::make_shared<VideoReceiveStatisticsObserverMock>()),
          frame_buffer_(std::make_unique<jitter::FrameBuffer>(time_controller_.Clock(), 
                                                              timing_,
                                                              decode_queue_,
                                                              stats_observer_)) {}

    uint32_t Rand() const {
        return utils::random::random<uint32_t>(0, 0x34678213);
    }

    void AdvanceTimeMs(int64_t duration_ms) {
        time_controller_.AdvanceTime(TimeDelta::Millis(duration_ms));
    }

    template<typename... U>
    FrameToDecode CreateFrame(uint16_t picture_id,
                              int64_t timestamp_ms,
                              int times_nacked,
                              size_t frame_size,
                              U... refs) {
        static_assert(sizeof...(refs) <= kMaxReferences,
                      "To many references specified for frame to decode.");
        std::array<uint16_t, sizeof...(refs)> references = {{utils::numeric::checked_static_cast<uint16_t>(refs)...}};
        VideoFrameType frame_type = references.size() == 0 ? VideoFrameType::KEY : VideoFrameType::DELTA;
        FakeFrameToDecode frame(frame_type, timestamp_ms, times_nacked, frame_size);
        frame.set_id(picture_id);
        for (uint16_t ref : references) {
            frame.InsertReference(ref);
        }
        return frame;
    }

    template<typename... U>
    int64_t InsertFrame(uint16_t picture_id,
                     int64_t timestamp_ms,
                     size_t frame_size,
                     U... refs) {
        return frame_buffer_->InsertFrame(CreateFrame(picture_id, timestamp_ms, 0, frame_size, refs...)).first;
    }

    int64_t InsertNackedFrame(uint16_t picture_id, int64_t timestamp_ms, int times_nacked = 1) {
        return frame_buffer_->InsertFrame(CreateFrame(picture_id, timestamp_ms, times_nacked, kFrameSize)).first;
    }
    // 
    void ExtractFrame(int64_t max_wait_time_ms = 0, bool keyframe_required = false) {
        decode_queue_->Async([this, max_wait_time_ms, keyframe_required](){
            frame_buffer_->NextFrame(max_wait_time_ms, keyframe_required, [this](std::optional<FrameToDecode> frame){
                if (frame) {
                    frames_.emplace_back(std::move(*frame));
                }
            });
        });
        if (max_wait_time_ms == 0) {
            AdvanceTimeMs(0);
        }
    }

    void CheckFrame(size_t index, int picture_id) {
        ASSERT_LT(index, frames_.size()) << "index: " << index;;
        ASSERT_NE(nullptr, frames_[index].cdata());
        ASSERT_EQ(picture_id, frames_[index].id()) << "index: " << index;
    }

    void CheckFrameSize(size_t index, size_t size) {
        ASSERT_LT(index, frames_.size());
        ASSERT_NE(nullptr, frames_[index].cdata());
        ASSERT_EQ(size, frames_[index].size());
    }

    void CheckNoFrame(size_t index) {
        ASSERT_GE(index, frames_.size());
    }

protected:
    SimulatedTimeController time_controller_;
    std::shared_ptr<TaskQueue> decode_queue_;
    std::shared_ptr<FakeTiming> timing_;
    std::shared_ptr<VideoReceiveStatisticsObserverMock> stats_observer_;
    std::unique_ptr<jitter::FrameBuffer> frame_buffer_;
    std::vector<FrameToDecode> frames_;
};

MY_TEST_F(FrameBufferTest, WaitForFrame) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();
    ExtractFrame(50);
    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(1);
    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    AdvanceTimeMs(50);
    CheckFrame(0, pid);
}

MY_TEST_F(FrameBufferTest, ExtractFromEmptyBuffer) {
    ExtractFrame();
    CheckNoFrame(0);
}

MY_TEST_F(FrameBufferTest, MissingFrame) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(3);

    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    // Missing the frame with id = pid + 1
    EXPECT_EQ(pid + 2, InsertFrame(pid + 2, ts, kFrameSize));
    EXPECT_EQ(pid + 2, InsertFrame(pid + 3, ts, kFrameSize, pid + 1, pid + 2));
    ExtractFrame();
    ExtractFrame();
    ExtractFrame();

    CheckFrame(0, pid);
    CheckFrame(1, pid + 2);
    CheckNoFrame(2);
}

MY_TEST_F(FrameBufferTest, FrameStream) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(10);
   
    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    ExtractFrame();
    CheckFrame(0, pid);
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(pid + i, InsertFrame(pid + i, ts + i * kFps10, kFrameSize, pid + i - 1));
        ExtractFrame();
        AdvanceTimeMs(kFps10);
        CheckFrame(i, pid + i);
    }
}

MY_TEST_F(FrameBufferTest, DropFrameSinceSlowDecoder) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(10);

    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    EXPECT_EQ(pid + 1, InsertFrame(pid + 1, ts + kFps20, kFrameSize, pid));
    for (int i = 2; i < 10; i += 2) {
        uint32_t ts_t10 = ts + i / 2 * kFps10;
        EXPECT_EQ(pid + i, InsertFrame(pid + i, ts_t10, kFrameSize, pid + i - 2));
        EXPECT_EQ(pid + i + 1, InsertFrame(pid + i + 1, ts_t10 + kFps20, kFrameSize, pid + i, pid + i - 1));
    }

    // pid   refs   render_ms     now_ms    docode_time_ms
    // 0:    none     50         0     - 25       = 25   // will be decoded as expected
    // 1:      0      100        70    - 25       = 5    // will be decoded as expected
    // 2:      0      150       140    - 25       = -15  // will be decoded as there are no remained frames to decode.
    // 3:     1,2     200       210    - 25       = -35  // will be droped as the decode not fast enough and having next frame (pid=4) to decode now.
    // 4:      2      250       210    - 25       = 15   // will be decoded as expected
    // 5:     3,4      -         -        -        -     // Undecodable as the referred frame was undecoded.
    // 6:      4      350       280    - 25       = 45   // will be decoded as expected
    // 7:     5,6      -        -        -         -     // Undecodable as the referred frame was undecoded.
    // 8:      6      450       350    - 25       = 75   // will be decoded as expected
    // 9:     7,8      -        -        -         -     // Undecodable as the referred frame was undecoded.

    EXPECT_CALL(*stats_observer_, OnDroppedFrames(1)).Times(3);
    
    for (int i = 0; i < 10; ++i) {
        ExtractFrame();
        AdvanceTimeMs(70);
    }

    CheckFrame(0, pid);
    CheckFrame(1, pid + 1);
    CheckFrame(2, pid + 2);
    CheckFrame(3, pid + 4);
    CheckFrame(4, pid + 6);
    CheckFrame(5, pid + 8);
    CheckNoFrame(6);
    CheckNoFrame(7);
    CheckNoFrame(8);
    CheckNoFrame(9);
}

MY_TEST_F(FrameBufferTest, DropFramesIfSystemIsStalled) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(4);
   
    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    EXPECT_EQ(pid + 1, InsertFrame(pid + 1, ts + 1 * kFps10, kFrameSize, pid));
    EXPECT_EQ(pid + 2, InsertFrame(pid + 2, ts + 2 * kFps10, kFrameSize, pid + 1));
    EXPECT_EQ(pid + 3, InsertFrame(pid + 3, ts + 3 * kFps10, kFrameSize));

    EXPECT_CALL(*stats_observer_, OnDroppedFrames(2)).Times(1);

    ExtractFrame();

    // Jump forward in time, simulating the system being stalled for some reason.
    AdvanceTimeMs(3 * kFps10);
    ExtractFrame();

    CheckFrame(0, pid);
    CheckFrame(1, pid + 3);
}

MY_TEST_F(FrameBufferTest, DroppedFramesCountedOnClear) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(5);

    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    for (int i = 1; i < 5; ++i) {
        EXPECT_EQ(pid + i, InsertFrame(pid + i,ts + i * kFps10, kFrameSize, pid + i - 1));
    }

    // All frames should be dropped when Clear is called.
    EXPECT_CALL(*stats_observer_, OnDroppedFrames(5)).Times(1);
    frame_buffer_->Clear();
    // Make the clear task executed.
    AdvanceTimeMs(0);
}

MY_TEST_F(FrameBufferTest, InsertLateFrame) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(2);
   
    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(pid + 2, InsertFrame(pid + 2, ts, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(pid + 2, InsertFrame(pid + 1, ts, kFrameSize, pid));
    ExtractFrame();

    CheckFrame(0, pid);
    CheckFrame(1, pid + 2);
    CheckNoFrame(2);
}
// 
MY_TEST_F(FrameBufferTest, ProtectionModeNackFEC) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    constexpr int64_t kRttMs = 200;
    frame_buffer_->UpdateRtt(kRttMs);

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(4);

    // Jitter estimate unaffected by RTT in this protection mode.
    frame_buffer_->set_protection_mode(jitter::ProtectionMode::NACK_FEC);
    InsertNackedFrame(pid, ts);
    InsertNackedFrame(pid + 1, ts + 100);
    InsertNackedFrame(pid + 2, ts + 200);
    InsertFrame(pid + 3, ts + 300, kFrameSize);
    ExtractFrame();
    ExtractFrame();
    ExtractFrame();
    ExtractFrame();
    ASSERT_EQ(4u, frames_.size());
    EXPECT_LT(timing_->GetCurrentJitter(), kRttMs);
}

MY_TEST_F(FrameBufferTest, ProtectionModeNack) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    constexpr int64_t kRttMs = 200;
    frame_buffer_->UpdateRtt(kRttMs);

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(4);

    // Jitter estimate includes RTT (after 3 retransmitted packets)
    frame_buffer_->set_protection_mode(jitter::ProtectionMode::NACK);
    InsertNackedFrame(pid, ts);
    InsertNackedFrame(pid + 1, ts + 100);
    InsertNackedFrame(pid + 2, ts + 200);
    InsertFrame(pid + 3, ts + 300, kFrameSize);
    ExtractFrame();
    ExtractFrame();
    ExtractFrame();
    ExtractFrame();
    ASSERT_EQ(4u, frames_.size());

    EXPECT_GT(timing_->GetCurrentJitter(), kRttMs);
}

MY_TEST_F(FrameBufferTest, NoContinuousFrame) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(1);

    EXPECT_EQ(-1, InsertFrame(pid + 1, ts, kFrameSize, pid));
}

MY_TEST_F(FrameBufferTest, LastContinuousFrameSingleLayer) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(5);

    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    EXPECT_EQ(pid, InsertFrame(pid + 2, ts, kFrameSize, pid + 1));
    EXPECT_EQ(pid + 2, InsertFrame(pid + 1, ts, kFrameSize, pid));
    EXPECT_EQ(pid + 2, InsertFrame(pid + 4, ts, kFrameSize, pid + 3));
    EXPECT_EQ(pid + 5, InsertFrame(pid + 5, ts, kFrameSize));
}

MY_TEST_F(FrameBufferTest, PictureIdJumpBack) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(3);

    EXPECT_EQ(pid, InsertFrame(pid, ts, kFrameSize));
    EXPECT_EQ(pid + 1, InsertFrame(pid + 1, ts + 1, kFrameSize, pid));
    ExtractFrame();
    CheckFrame(0, pid);

    // pid + 1 will be cleared 
    EXPECT_CALL(*stats_observer_, OnDroppedFrames(1)).Times(1);

    // Jump back in pid but increase ts.
    EXPECT_EQ(pid - 1, InsertFrame(pid - 1, ts + 2, kFrameSize));
    ExtractFrame();
    ExtractFrame();
    CheckFrame(1, pid - 1);
    CheckNoFrame(2);
}

MY_TEST_F(FrameBufferTest, ForwardJumps) {
    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(8);

    EXPECT_EQ(5453, InsertFrame(5453, 1, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(5454, InsertFrame(5454, 1, kFrameSize, 5453));
    ExtractFrame();
    EXPECT_EQ(15670, InsertFrame(15670, 1, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(29804, InsertFrame(29804, 1, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(29805, InsertFrame(29805, 1, kFrameSize, 29804));
    ExtractFrame();
    EXPECT_EQ(29806, InsertFrame(29806, 1, kFrameSize, 29805));
    ExtractFrame();
    EXPECT_EQ(33819, InsertFrame(33819, 1, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(41248, InsertFrame(41248, 1, kFrameSize));
    ExtractFrame();
}

MY_TEST_F(FrameBufferTest, DuplicateFrames) {

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(1);

    EXPECT_EQ(22256, InsertFrame(22256, 1, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(22256, InsertFrame(22256, 1, kFrameSize));
}

MY_TEST_F(FrameBufferTest, InvalidReferences) {

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(2);

    EXPECT_EQ(-1, InsertFrame(0, 1000, kFrameSize, 2));
    EXPECT_EQ(1, InsertFrame(1, 2000, kFrameSize));
    ExtractFrame();
    EXPECT_EQ(2, InsertFrame(2, 3000, kFrameSize, 1));
}

MY_TEST_F(FrameBufferTest, KeyframeRequired) {

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(3);

    EXPECT_EQ(1, InsertFrame(1, 1000, kFrameSize));
    EXPECT_EQ(2, InsertFrame(2, 2000, kFrameSize, 1));
    EXPECT_EQ(3, InsertFrame(3, 3000, kFrameSize));

    // The delta frame with pid = 2 will be dropped.
    EXPECT_CALL(*stats_observer_, OnDroppedFrames(1)).Times(1);

    ExtractFrame();
    ExtractFrame(0, true);
    ExtractFrame();

    CheckFrame(0, 1);
    CheckFrame(1, 3);
    CheckNoFrame(2);
}

MY_TEST_F(FrameBufferTest, KeyframeClearsFullBuffer) {
    const int kMaxBufferSize = 600;

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(kMaxBufferSize + 1);
    EXPECT_CALL(*stats_observer_, OnDroppedFrames(kMaxBufferSize)).Times(1);

    for (int i = 1; i <= kMaxBufferSize; ++i)
        EXPECT_EQ(-1, InsertFrame(i, i * 1000, kFrameSize, i - 1));
    ExtractFrame();
    CheckNoFrame(0);

    EXPECT_EQ(kMaxBufferSize + 1, InsertFrame(kMaxBufferSize + 1, (kMaxBufferSize + 1) * 1000, kFrameSize));
    ExtractFrame();
    CheckFrame(0, kMaxBufferSize + 1);
}

MY_TEST_F(FrameBufferTest, DontDecodeOlderTimestamp) {

    EXPECT_CALL(*stats_observer_, OnCompleteFrame(_, _)).Times(4);
    // Frame 2 will be dropped before decoding the frame 3.
    EXPECT_CALL(*stats_observer_, OnDroppedFrames(1)).Times(1);

    InsertFrame(2, 1, kFrameSize);
    InsertFrame(1, 2, kFrameSize);  // Older picture id but newer timestamp.
    ExtractFrame(0);
    ExtractFrame(0);
    CheckFrame(0, 1);
    CheckNoFrame(1);

    InsertFrame(3, 4, kFrameSize);
    InsertFrame(4, 3, kFrameSize);  // Newer picture id but older timestamp.
    ExtractFrame(0);
    ExtractFrame(0);
    CheckFrame(1, 3);
    CheckNoFrame(2);
}
    
} // namespace test
} // namespace naivertc

