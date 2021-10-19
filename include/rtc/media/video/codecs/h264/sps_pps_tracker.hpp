#ifndef _RTC_MEDIA_VIDEO_CODECS_H264_SPS_PPS_TRACKER_H_
#define _RTC_MEDIA_VIDEO_CODECS_H264_SPS_PPS_TRACKER_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp_video_header.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"

#include <map>
#include <memory>
#include <vector>

namespace naivertc {
namespace h264 {

class RTC_CPP_EXPORT SpsPpsTracker {
public:
    enum class PacketAction { INSERT, DROP, REQUEST_KEY_FRAME };
    struct FixedBitstream {
        PacketAction action;
        CopyOnWriteBuffer bitstream;
    };
public:
    SpsPpsTracker();
    ~SpsPpsTracker();

    FixedBitstream CopyAndFixBitstream(bool is_first_packet_in_frame,
                                       uint16_t& fixed_frame_width,
                                       uint16_t& fixed_frame_height,
                                       h264::PacketizationInfo& h264_header,
                                       ArrayView<const uint8_t> bitstream);
    void InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                           const std::vector<uint8_t>& pps);
private:    
    struct PpsInfo {
        PpsInfo();
        PpsInfo(PpsInfo&& rhs);
        PpsInfo& operator=(PpsInfo&& rhs);
        ~PpsInfo();

        int sps_id = -1;
        size_t size = 0;
        std::unique_ptr<uint8_t[]> data;
    };

    struct SpsInfo {
        SpsInfo();
        SpsInfo(SpsInfo&& rhs);
        SpsInfo& operator=(SpsInfo&& rhs);
        ~SpsInfo();

        size_t size = 0;
        uint16_t width = -1;
        uint16_t height = -1;
        std::unique_ptr<uint8_t[]> data;
    };

    std::map<uint32_t, PpsInfo> pps_data_;
    std::map<uint32_t, SpsInfo> sps_data_;
};

} // namespace h264
} // namespace naivertc

#endif