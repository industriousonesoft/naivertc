#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/base/time/clock_simulated.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "common/utils_numeric.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtp::video;

namespace naivertc {
namespace test {

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

    int64_t MaxTimeWaitingToDecode(int64_t render_time_ms, int64_t now_ms) override {
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
    static const int kDecodeTimeMs = kDelayMs /2;
    mutable uint32_t last_timestamp_ = 0;
    mutable int64_t last_ms_ = -1;
};

// FakeFrameToDecode
class FakeFrameToDecode : public FrameToDecode {
public:
    FakeFrameToDecode(int64_t timestamp_ms,
                      int times_nacked, 
                      size_t frame_size) 
        : FrameToDecode(VideoFrameType::DELTA,
                        VideoCodecType::H264,
                        0, /* seq_num_start */
                        0, /* seq_num_end */
                        timestamp_ms * 90, /* timestamp */
                        timestamp_ms, /* ntp_time_ms */
                        times_nacked, /* times_nacked */
                        0, /* min_received_time_ms */
                        0, /* max_received_time_ms */
                        CopyOnWriteBuffer(frame_size)) {}

    
};

// FrameBufferTest
class FrameBufferTest : public ::testing::Test {
protected:
    FrameBufferTest() 
        : clock_(std::shared_ptr<Clock>(new SimulatedClock(0))),
          timing_(std::shared_ptr<Timing>(new FakeTiming(clock_))),
          frame_buffer_(std::make_unique<jitter::FrameBuffer>(clock_, timing_)) {}

    template<typename... T>
    std::unique_ptr<FrameToDecode> CreateFrame(uint16_t picture_id,
                                               int64_t timestamp_ms,
                                               int times_nacked,
                                               size_t frame_size,
                                               T... refs) {
        static_assert(sizeof...(refs) <= kMaxReferences,
                      "To many references specified for frame to decode.");
        std::array<uint16_t, sizeof...(refs)> references = {{utils::numeric::checked_static_cast<uint16_t>(refs)...}};

        auto frame = std::make_unique<FakeFrameToDecode>(timestamp_ms,
                                                         0, /* times_nacked */
                                                         frame_size);
        frame->set_id(picture_id);
        for (uint16_t ref : references) {
            frame->InsertReference(ref);
        }
        return frame;
    }

    template<typename... T>
    bool InsertFrame(uint16_t picture_id,
                     int64_t timestamp_ms,
                     size_t frame_size,
                     T... refs) {
        return frame_buffer_->InsertFrame(CreateFrame(picture_id, timestamp_ms, 0, frame_size, refs...));
    }

    bool InsertNackedFrame(uint16_t picture_id, int64_t timestamp_ms, int times_nacked = 1) {
        return frame_buffer_->InsertFrame(CreateFrame(picture_id, timestamp_ms, times_nacked, kFrameSize));
    }


protected:
    static const int kMaxReferences = 5;
    static const int kFps1 = 10;
    static const int kFps10 = kFps1 / 10;
    static const int kFps20 = kFps1 / 20;
    static const size_t kFrameSize = 10;

protected:
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<Timing> timing_;
    std::unique_ptr<jitter::FrameBuffer> frame_buffer_;

};
    
} // namespace test
} // namespace naivertc

