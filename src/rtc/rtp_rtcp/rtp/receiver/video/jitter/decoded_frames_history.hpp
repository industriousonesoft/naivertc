#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_DECODED_FRAMES_HISTORY_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_DECODED_FRAMES_HISTORY_H_

#include "base/defines.hpp"

#include <vector>
#include <bitset>
#include <optional>

namespace naivertc {
namespace rtp {
namespace video {

class DecodedFramesHistory {
public:
    explicit DecodedFramesHistory(size_t window_size);
    ~DecodedFramesHistory();

    std::optional<int64_t> last_decoded_frame_id() const { return last_decoded_frame_id_; }
    std::optional<uint32_t> last_decoded_frame_timestamp() const { return last_decoded_frame_timestamp_; }

    void InsertFrame(int64_t frame_id, uint32_t timestamp);
    bool WasDecoded(int64_t frame_id);
    void Clear();

private:
    int FrameIdToIndex(int64_t frame_id) const;
private:
    const size_t window_size_;

    std::vector<bool> buffer_;
    std::optional<int64_t> last_decoded_frame_id_;
    std::optional<uint32_t> last_decoded_frame_timestamp_;
};

} // namespace video
} // namespace rtp
} // namespace naivert 

#endif