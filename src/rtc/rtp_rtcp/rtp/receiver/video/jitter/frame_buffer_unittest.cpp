#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/base/time/clock_simulated.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "common/utils_numeric.hpp"
#include "common/utils_random.hpp"
#include "common/task_queue.hpp"
#include "common/event.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

using namespace naivertc::rtp::video;

namespace naivertc {
namespace test {
constexpr int kMaxReferences = 5;
constexpr int kFps1 = 1000;         // 10ms
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
    static const int kDecodeTimeMs = kDelayMs / 2;
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
                        timestamp_ms, /* ntp_time_ms */
                        times_nacked, /* times_nacked */
                        0, /* min_received_time_ms */
                        0 /* max_received_time_ms */) {}

    
};

// FakeTaskQueue
class TaskQueueForTest : public TaskQueue {
public:
    void Async(std::function<void()> handler) const override {
        Event event;
        TaskQueue::Async([&event, handler=std::move(handler)](){
            handler();
            event.Set();
        });
        event.Wait(Event::kForever);
    }

    void AsyncAfter(TimeInterval delay_in_sec, std::function<void()> handler) override {
        Event event;
        TaskQueue::AsyncAfter(delay_in_sec, [&event, handler=std::move(handler)](){
            handler();
            event.Set();
        });
        event.Wait(Event::kForever);
    }
};

// FrameBufferTest
class T(FrameBufferTest) : public ::testing::Test {
protected:
    T(FrameBufferTest)() 
        : clock_(std::make_shared<SimulatedClock>(0)),
          timing_(std::make_shared<FakeTiming>(clock_)),
          task_queue_(std::make_shared<TaskQueueForTest>()),
          decode_queue_(std::make_shared<TaskQueueForTest>()),
          stats_observer_(nullptr /* std::make_shared<VideoReceiveStatisticsObserverMock>() */),
          frame_buffer_(std::make_unique<jitter::FrameBuffer>(jitter::ProtectionMode::NACK_FEC, clock_, timing_, task_queue_, decode_queue_, stats_observer_)) {
        frame_buffer_->OnDecodableFrame([this](FrameToDecode frame, int64_t wait_ms){
            EXPECT_GT(wait_ms, 0) << wait_ms;
            frames_.emplace_back(std::move(frame));
        });
    }

    uint16_t RandPid() const {
        return utils::random::generate_random<uint16_t>();
    }

    uint32_t RandTs() const {
        return utils::random::generate_random<uint32_t>();
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

    void CheckFrame(size_t index, int picture_id) {
        ASSERT_LT(index, frames_.size());
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
    std::shared_ptr<Timing> timing_;
    std::shared_ptr<TaskQueue> task_queue_;
    std::shared_ptr<TaskQueue> decode_queue_;
    std::shared_ptr<VideoReceiveStatisticsObserverMock> stats_observer_;
    std::unique_ptr<jitter::FrameBuffer> frame_buffer_;
    std::vector<FrameToDecode> frames_;
};

MY_TEST_F(FrameBufferTest, WaitForFrame) {
    uint16_t pid = RandPid();
    uint32_t ts = RandTs();

    InsertFrame(pid, ts, kFrameSize);
    CheckFrame(0, pid);
}

// MY_TEST_F(FrameBufferTest, MissingFrame) {
//     uint16_t pid = RandPid();
//     uint32_t ts = RandTs();

//     InsertFrame(pid, ts, kFrameSize);
//     InsertFrame(pid + 2, ts, kFrameSize);
//     // Missing pid + 1
//     InsertFrame(pid + 3, ts, kFrameSize, pid + 1, pid + 2);

//     CheckFrame(0, pid);
//     CheckFrame(1, pid + 2);
//     CheckNoFrame(2);
// }

// MY_TEST_F(FrameBufferTest, FrameStream) {
//     uint16_t pid = RandPid();
//     uint32_t ts = RandTs();

//     InsertFrame(pid, ts, kFrameSize);
//     CheckFrame(0, pid);
//     for (int i = 1; i < 10; ++i) {
//         InsertFrame(pid + i, ts + i * kFps10, kFrameSize, pid + i - 1);
//         clock_->AdvanceTimeMs(kFps10);
//         CheckFrame(i, pid + i);
//     }
// }

// MY_TEST_F(FrameBufferTest, DropFrameSinceSlowDecoder) {
//     uint16_t pid = 1;
//     uint32_t ts = 1234;

//     // EXPECT_CALL(*stats_observer_, OnDroppedFrames(1)).Times(3);

//     InsertFrame(pid, ts, kFrameSize);
//     InsertFrame(pid + 1, ts + kFps20, kFrameSize);
//     for (int i = 2; i < 10; i += 2) {
//         // i = 2,4,6,8
//         uint32_t ts_t10 = ts + i / 2 * kFps10;
//         InsertFrame(pid + i, ts_t10, kFrameSize, pid + i - 2);
//         InsertFrame(pid + i + 1, ts_t10 + kFps20, kFrameSize, pid + i, pid + i - 1);
//     }

//     CheckFrame(0, pid);
//     CheckFrame(1, pid + 1);
//     CheckFrame(2, pid + 2);
//     CheckFrame(3, pid + 4);
//     CheckFrame(4, pid + 6);
//     CheckFrame(5, pid + 8);
//     // CheckNoFrame(6);
//     // CheckNoFrame(7);
//     // CheckNoFrame(8);
//     // CheckNoFrame(9);
// }
    
} // namespace test
} // namespace naivertc

