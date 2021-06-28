#ifndef _PC_SDP_ENTRY_H_
#define _PC_SDP_ENTRY_H_

#include "base/defines.hpp"
#include "pc/sdp_defines.hpp"

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace naivertc {
namespace sdp {

// Entry
class RTC_CPP_EXPORT Entry {
public:
    virtual ~Entry() = default;

    virtual std::string type() const { return type_; }
    virtual std::string description() const { return description_; }
    virtual std::string mid() const { return mid_; };
    virtual void ParseSDPLine(std::string_view line);

    Direction direction() const { return direction_; }
    void set_direction(Direction direction);

    std::string GenerateSDP(std::string_view eol, std::string addr, std::string_view port) const;

protected:
    Entry(const std::string& mline, std::string mid, Direction direction = Direction::UNKNOWN);
    virtual std::string GenerateSDPLines(std::string_view eol) const;   

    std::vector<std::string> attributes_;

private:
    std::string type_;
    std::string description_;
    std::string mid_;
    Direction direction_;
};

// Application
class RTC_CPP_EXPORT Application : public Entry {
public:
    Application(std::string mid = "data");
    virtual ~Application() = default;

    std::string description() const override;

    std::optional<uint16_t> sctp_port() const { return sctp_port_; }
    void set_sctp_port(uint16_t port) { sctp_port_ = port; }
    void HintSctpPort(uint16_t port) { sctp_port_ = sctp_port_.value_or(port); }

    std::optional<size_t> max_message_size() const { return max_message_size_; }
    void set_max_message_size(size_t size) { max_message_size_ = size; }

    virtual void ParseSDPLine(std::string_view line) override;

private:

    virtual std::string GenerateSDPLines(std::string_view eol) const override;

    std::optional<uint16_t> sctp_port_;
    std::optional<size_t> max_message_size_;
};

// Media
class RTC_CPP_EXPORT Media : public Entry {
public:
    struct RTPMap {
    public:
        RTPMap(std::string_view mline);
        RTPMap() {}

        void AddFeedback(const std::string& line);
        void RemoveFeedback(const std::string& line);
        void AddAttribute(std::string attr) { fmt_profiles.emplace_back(std::move(attr)); }

        static int ParsePayloadType(std::string_view pt);
        void SetMLine(std::string_view mline);

    public:
        std::vector<std::string> rtcp_feedbacks;
        std::vector<std::string> fmt_profiles;
        int pt;
        std::string format;
        int clock_rate;
        std::string codec_params;
    };
public:
    Media(const std::string& sdp);
    Media(const std::string& mline, std::string mid, Direction direction = Direction::SEND_ONLY);
    virtual ~Media() = default;

    std::string description() const override;

    void set_bandwidth_max_value(int value);
    int bandwidth_max_value();

    void AddSSRC(uint32_t ssrc, std::optional<std::string> name, std::optional<std::string> msid = std::nullopt, std::optional<std::string> track_id = std::nullopt);
    void RemoveSSRC(uint32_t ssrc);
    void ReplaceSSRC(uint32_t old_ssrc, uint32_t ssrc, std::optional<std::string> name, std::optional<std::string> msid = std::nullopt, std::optional<std::string> track_id = std::nullopt);
    bool HasSSRC(uint32_t ssrc);
    std::vector<uint32_t> GetSSRCS();
    std::optional<std::string> GetCNameForSSRC(uint32_t ssrc);

    bool HasPayloadType(int pt) const;

    virtual void ParseSDPLine(std::string_view line) override;

    void AddRTPMap(const RTPMap& map);

private:
    std::map<int, RTPMap> rtp_map_;
    std::vector<uint32_t> ssrcs_;
    std::map<uint32_t, std::string> cname_map_;

    int bandwidth_max_value_ = -1;
};

// Audio 
class RTC_CPP_EXPORT Audio : public Media {
public:
    Audio(std::string mid="audio", Direction direction = Direction::SEND_ONLY);

    void AddAudioCodec(int payload_type, std::string codec, int clock_rate = 48000, int channels = 2, std::optional<std::string> profile = std::nullopt);
    
    void AddOpusCodec(int payload_type, std::optional<std::string> profile = DEFAULT_OPUS_AUDIO_PROFILE);
};

// Video
class RTC_CPP_EXPORT Video : public Media {
public: 
    Video(std::string mid = "video", Direction direction = Direction::SEND_ONLY);

    void AddVideoCodec(int payload_type, std::string codec, std::optional<std::string> profile = std::nullopt);

    void AddH264Codec(int payload_type, std::optional<std::string> profile = DEFAULT_H264_VIDEO_PROFILE);
};

}
}

#endif