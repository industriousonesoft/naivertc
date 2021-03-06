#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp"
#include "common/utils_random.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

#include <map>

using namespace naivertc::rtp::video;

namespace naivertc {
namespace test {
namespace {
FrameToDecode CreateFrame(uint16_t seq_num_start, 
                          uint16_t seq_num_end,
                          bool is_keyframe,
                          video::CodecType codec_type) {
    video::FrameType frame_type = is_keyframe ? video::FrameType::KEY : video::FrameType::DELTA;
    return FrameToDecode(CopyOnWriteBuffer(1),
                         frame_type, 
                         codec_type, 
                         seq_num_start, 
                         seq_num_end, 
                         0, /* timestamp */
                         0, /* ntp_time_ms */
                         -1, /* times_nacked */
                         -1, /* min_received_time_ms */
                         -1 /* max_received_time_ms */);
}
} // namespace

class FrameRefFinderTest : public ::testing::Test {
protected:
    FrameRefFinderTest(video::CodecType codec_type) 
        : frame_ref_finder_(jitter::FrameRefFinder::Create(codec_type)) {
        frame_ref_finder_->OnFrameRefFound([this](FrameToDecode frame){
            OnFrameRefFound(std::move(frame));
        });
    }

    uint16_t Rand() const { return utils::random::generate_random<uint16_t>(); }

    void InsertH264(uint16_t seq_num_start, uint16_t seq_num_end, bool is_keyframe) {
        auto frame = CreateFrame(seq_num_start, seq_num_end, is_keyframe, video::CodecType::H264);
        EXPECT_NE(frame.cdata(), nullptr);
        frame_ref_finder_->InsertFrame(std::move(frame));
    }

    void InsertPadding(uint16_t seq_num) {
        frame_ref_finder_->InsertPadding(seq_num);
    }

    void OnFrameRefFound(FrameToDecode frame) {
        int64_t pid = frame.id();
        auto frame_it = referred_frames_.find(pid);
        if (frame_it != referred_frames_.end()) {
            ADD_FAILURE() << "Already received frame with pid=" << pid;
            return;
        }
        referred_frames_.insert({pid, std::move(frame)});
    }

    template <typename... T>
    void CheckReferencesH264(int64_t pid, T... refs) const {
        CheckReferences(pid, refs...);
    }

private:
    template<typename... T>
    void CheckReferences(int64_t pid, T... refs) const {
        auto frame_it = referred_frames_.find(pid);
        if (frame_it == referred_frames_.end()) {
            ADD_FAILURE() << "Could not find frame with pid=" << pid;
            return;
        }
        std::set<int64_t> actual_refs;
        frame_it->second.ForEachReference([&actual_refs](int64_t pid, bool* stoped){
            actual_refs.insert(pid);
        });
        std::set<int64_t> expected_refs;
        RefsToSet(&expected_refs, refs...);

        ASSERT_EQ(expected_refs, actual_refs);
    }

    template <typename... T>
    void RefsToSet(std::set<int64_t>* m, int64_t ref, T... refs) const {
        m->insert(ref);
        RefsToSet(m, refs...);
    }

    void RefsToSet(std::set<int64_t>* m) const {}

protected:
    std::unique_ptr<jitter::FrameRefFinder> frame_ref_finder_;
    struct FrameComp {
        bool operator()(int64_t p1, int64_t p2) const {
            return p1 < p2;
        }
    };
    std::map<int64_t, FrameToDecode> referred_frames_;
};

// H264
class T(H264FrameRefFinderTest) : public FrameRefFinderTest {
protected:
    T(H264FrameRefFinderTest)() : FrameRefFinderTest(video::CodecType::H264) {}
};

MY_TEST_F(H264FrameRefFinderTest, H264KeyFrameReferences) {
    uint16_t seq_num = Rand();
    InsertH264(seq_num, seq_num, true);

    ASSERT_EQ(1UL, referred_frames_.size());
    CheckReferencesH264(seq_num);
}

MY_TEST_F(H264FrameRefFinderTest, H264SequenceNumberWrap) {
    uint16_t seq_num = 0xFFFF;

    InsertH264(seq_num - 1, seq_num - 1, true);
    InsertH264(seq_num, seq_num, false);
    InsertH264(seq_num + 1, seq_num + 1, false);
    InsertH264(seq_num + 2, seq_num + 2, false);

    ASSERT_EQ(4UL, referred_frames_.size());
    CheckReferencesH264(seq_num - 1);
    CheckReferencesH264(seq_num, seq_num - 1);
    CheckReferencesH264(seq_num + 1, seq_num);
    CheckReferencesH264(seq_num + 2, seq_num + 1);
}

MY_TEST_F(H264FrameRefFinderTest, H264Frames) {
    uint16_t seq_num = Rand();

    InsertH264(seq_num, seq_num, true);
    InsertH264(seq_num + 1, seq_num + 1, false);
    InsertH264(seq_num + 2, seq_num + 2, false);
    InsertH264(seq_num + 3, seq_num + 3, false);

    ASSERT_EQ(4UL, referred_frames_.size());
    CheckReferencesH264(seq_num);
    CheckReferencesH264(seq_num + 1, seq_num);
    CheckReferencesH264(seq_num + 2, seq_num + 1);
    CheckReferencesH264(seq_num + 3, seq_num + 2);
}

MY_TEST_F(H264FrameRefFinderTest, H264Reordering) {
    uint16_t seq_num = 0;//Rand();

    InsertH264(seq_num, seq_num, true);
    InsertH264(seq_num + 1, seq_num + 1, false);
    InsertH264(seq_num + 3, seq_num + 3, false);
    InsertH264(seq_num + 2, seq_num + 2, false);
    InsertH264(seq_num + 5, seq_num + 5, false);
    InsertH264(seq_num + 6, seq_num + 6, false);
    InsertH264(seq_num + 4, seq_num + 4, false);

    ASSERT_EQ(7UL, referred_frames_.size());
    CheckReferencesH264(seq_num);
    CheckReferencesH264(seq_num + 1, seq_num);
    CheckReferencesH264(seq_num + 2, seq_num + 1);
    CheckReferencesH264(seq_num + 3, seq_num + 2);
    CheckReferencesH264(seq_num + 4, seq_num + 3);
    CheckReferencesH264(seq_num + 5, seq_num + 4);
    CheckReferencesH264(seq_num + 6, seq_num + 5);
}

MY_TEST_F(H264FrameRefFinderTest, H264SequenceNumberWrapMulti) {
    uint16_t seq_num = 0xFFFF;

    InsertH264(seq_num - 3, seq_num - 2, true);
    InsertH264(seq_num - 1, seq_num + 1, false);
    InsertH264(seq_num + 2, seq_num + 3, false);
    InsertH264(seq_num + 4, seq_num + 7, false);

    ASSERT_EQ(4UL, referred_frames_.size());
    CheckReferencesH264(seq_num - 2);
    CheckReferencesH264(seq_num + 1, seq_num - 2);
    CheckReferencesH264(seq_num + 3, seq_num + 1);
    CheckReferencesH264(seq_num + 7, seq_num + 3);
}
    
} // namespace test
} // namespace naivertc
