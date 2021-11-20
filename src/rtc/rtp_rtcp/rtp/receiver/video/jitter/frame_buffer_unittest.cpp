#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/base/time/clock_simulated.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "common/utils_numeric.hpp"
#include "common/utils_random.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/base/synchronization/event.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

using namespace naivertc::rtp::video;

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
        : clock_(std::make_shared<SimulatedClock>(0)),
          timing_(std::make_shared<FakeTiming>(clock_)),
          task_queue_(std::make_shared<TaskQueue>()),
          decode_queue_(std::make_shared<TaskQueue>()),
          stats_observer_(/*std::make_shared<VideoReceiveStatisticsObserverMock>()*/ nullptr),
          frame_buffer_(std::make_unique<jitter::FrameBuffer>(jitter::ProtectionMode::NACK_FEC, 
                                                              clock_, 
                                                              timing_, 
                                                              task_queue_, 
                                                              decode_queue_, 
                                                              stats_observer_)) {}

    uint32_t Rand() const {
        return utils::random::random<uint32_t>(0, 0x34678213);
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
        FakeFrameToDecode frame(frame_type, timestamp_ms, 0, /* times_nacked */frame_size);
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

    void ExtractFrame(int64_t max_wait_time_ms = 0, bool keyframe_required = false, std::function<void(void)> on_finished = nullptr) {
        frame_buffer_->NextFrame(max_wait_time_ms, keyframe_required, [this, on_finished=std::move(on_finished)](std::optional<FrameToDecode> frame){
            if (frame) {
                frames_.emplace_back(std::move(*frame));
            }
            if (on_finished) {
                on_finished();
            }
        });
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
    std::shared_ptr<SimulatedClock> clock_;
    std::shared_ptr<FakeTiming> timing_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::shared_ptr<TaskQueue> decode_queue_;
    std::shared_ptr<VideoReceiveStatisticsObserverMock> stats_observer_;
    std::unique_ptr<jitter::FrameBuffer> frame_buffer_;
    std::vector<FrameToDecode> frames_;
};

MY_TEST_F(FrameBufferTest, WaitForFrame) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();
    Event event;
    ExtractFrame(50, false, [&event](){
        event.Set();
    });
    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    clock_->AdvanceTimeMs(50);
    event.WaitForever();
    CheckFrame(0, pid);
}

MY_TEST_F(FrameBufferTest, ExtractFromEmptyBuffer) {
    Event event;
    ExtractFrame(0, false, [&event](){
        event.Set();
    });
    event.WaitForever();
    CheckNoFrame(0);
}

MY_TEST_F(FrameBufferTest, MissingFrame) {
    Event event;
    uint16_t pid = Rand();
    uint32_t ts = Rand();
    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    // Missing the frame with id = pid + 1
    EXPECT_EQ(InsertFrame(pid + 2, ts, kFrameSize), pid + 2);
    EXPECT_EQ(InsertFrame(pid + 3, ts, kFrameSize, pid + 1, pid + 2), pid + 2);
    ExtractFrame(0, false, [&](){
        ExtractFrame(0, false, [&](){
            ExtractFrame(0, false, [&](){
                event.Set();
            });
        });
    });
    event.WaitForever();
    CheckFrame(0, pid);
    CheckFrame(1, pid + 2);
    CheckNoFrame(2);
}

MY_TEST_F(FrameBufferTest, FrameStream) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();
    Event event;
    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    ExtractFrame(0, false, [&](){
        event.Set();
    });
    event.WaitForever();
    CheckFrame(0, pid);
    for (int i = 1; i < 10; ++i) {
        event.Reset();
        EXPECT_EQ(InsertFrame(pid + i, ts + i * kFps10, kFrameSize, pid + i - 1), pid + i);
        ExtractFrame(0, false, [&](){
            event.Set();
        });
        clock_->AdvanceTimeMs(kFps10);
        event.WaitForever();
        CheckFrame(i, pid + i);
    }
}

MY_TEST_F(FrameBufferTest, DISABLED_DropFrameSinceSlowDecoder) {
    uint16_t pid = 0;
    uint32_t ts = 0;

    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    EXPECT_EQ(InsertFrame(pid + 1, ts + kFps20, kFrameSize, pid), pid + 1);
    for (int i = 2; i < 10; i += 2) {
        uint32_t ts_t10 = ts + i / 2 * kFps10;
        EXPECT_EQ(InsertFrame(pid + i, ts_t10, kFrameSize, pid + i - 2), pid + i);
        EXPECT_EQ(InsertFrame(pid + i + 1, ts_t10 + kFps20, kFrameSize, pid + i, pid + i - 1), pid + i + 1);
    }

    // EXPECT_CALL(*stats_observer_, OnDroppedFrames(1)).Times(3);

    //    render     now    docode
    // 0:  50         0     - 25 = 25
    // 1:  100        70    - 25 = 5
    // 2:  150       140    - 25 = -15
    // 3:  200       210    - 25 = -35
    // 4:  250       280    - 25 = -55
    // 5:  300       350    - 25 = -75
    
    for (int i = 0; i < 10; ++i) {
        Event event;
        ExtractFrame(0, false, [&](){
            event.Set();
        });
        event.WaitForever();
        clock_->AdvanceTimeMs(70);
    }

    CheckFrame(0, pid);
    CheckFrame(1, pid + 1);
    CheckFrame(2, pid + 2);
    CheckFrame(3, pid + 4);
    CheckFrame(4, pid + 6);
    CheckFrame(5, pid + 8);
    // CheckNoFrame(6);
    // CheckNoFrame(7);
    // CheckNoFrame(8);
    // CheckNoFrame(9);
}

MY_TEST_F(FrameBufferTest, DropFramesIfSystemIsStalled) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    EXPECT_EQ(InsertFrame(pid + 1, ts + 1 * kFps10, kFrameSize, pid), pid + 1);
    EXPECT_EQ(InsertFrame(pid + 2, ts + 2 * kFps10, kFrameSize, pid + 1), pid + 2);
    EXPECT_EQ(InsertFrame(pid + 3, ts + 3 * kFps10, kFrameSize), pid + 3);

    Event event;
    ExtractFrame(0, false, [&](){
        clock_->AdvanceTimeMs(3 * kFps10);
        ExtractFrame(0, false, [&](){
            event.Set();
        });
    });
    event.WaitForever();

    CheckFrame(0, pid);
    CheckFrame(1, pid + 3);
}

MY_TEST_F(FrameBufferTest, DISABLED_DroppedFramesCountedOnClear) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    for (int i = 1; i < 5; ++i) {
        EXPECT_EQ(InsertFrame(pid + i,ts + i * kFps10, kFrameSize, pid + i - 1), pid + i);
    }

    // All frames should be dropped when Clear is called.
    // EXPECT_CALL(*stats_observer_, OnDroppedFrames(5)).Times(1);
    // frame_buffer_->Clear();
}

MY_TEST_F(FrameBufferTest, InsertLateFrame) {
    uint16_t pid = Rand();
    uint32_t ts = Rand();

    Event event;
    EXPECT_EQ(InsertFrame(pid, ts, kFrameSize), pid);
    ExtractFrame(0, false, [&](){
        event.Set();
    });
    event.WaitForever();

    event.Reset();
    EXPECT_EQ(InsertFrame(pid + 2, ts, kFrameSize), pid + 2);
    ExtractFrame(0, false, [&](){
        event.Set();
    });
    event.WaitForever();

    event.Reset();
    EXPECT_EQ(InsertFrame(pid + 1, ts, kFrameSize, pid), pid + 2);
    ExtractFrame(0, false, [&](){
        event.Set();
    });
    event.WaitForever();

    CheckFrame(0, pid);
    CheckFrame(1, pid + 2);
    CheckNoFrame(2);
}
    
} // namespace test
} // namespace naivertc

