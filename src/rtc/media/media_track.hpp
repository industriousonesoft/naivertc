#ifndef _RTC_MEDIA_TRACK_H_
#define _RTC_MEDIA_TRACK_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/media/media_channel.hpp"
#include "rtc/base/task_utils/task_queue.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <iostream>

namespace naivertc {

class Broadcaster;

class RTC_CPP_EXPORT MediaTrack : public MediaChannel {
public:
    using Direction = sdp::Direction;

    // Kind
    enum class Kind {
        VIDEO,
        AUDIO
    };

    enum class Codec {
        H264,
        OPUS
    };

    enum class FecCodec {
        // UlpFec + Red
        ULP_FEC,
        // FlexFec
        FLEX_FEC
    };

    enum class CongestionControl {
        // goog-remb
        GOOG_REMB,
        // transport-cc
        TRANSPORT_CC
    };

    struct CodecParams {
        Codec codec;
        // TODO: Add more attributes, e.g.: clock rate, channels.
        std::optional<std::string> profile = std::nullopt;

        CodecParams(Codec codec,
                    std::optional<std::string> profile = std::nullopt);
    };

    // Configuration
    struct Configuration {
    public:
        Configuration(Kind kind, std::string mid);

        Kind kind() const { return kind_; };
        std::string mid() const { return mid_; }
        void set_mid(std::string mid) { mid_ = std::move(mid); }

        // Media codec
        bool AddCodec(CodecParams cp);
        bool AddCodec(Codec codec, std::optional<std::string> profile = std::nullopt);
        void RemoveCodec(Codec codec, std::optional<std::string> profile = std::nullopt);
        void ForEachCodec(std::function<void(const CodecParams& cp)>&& handler) const;
        std::vector<CodecParams> media_codecs() const { return media_codecs_; }
        
    public:
        Direction direction = Direction::SEND_RECV;
        // RTX: Used for Nack packet or Padding packet
        bool rtx_enabled = false;
        // Feedbacks
        bool nack_enabled = false;
        std::optional<CongestionControl> congestion_control = std::nullopt;
        // FEC codec
        std::optional<FecCodec> fec_codec = std::nullopt;

        std::optional<std::string> cname = std::nullopt;
        std::optional<std::string> msid = std::nullopt;
        std::optional<std::string> track_id = std::nullopt;
    
    private:
        Kind kind_;
        std::string mid_;
    
        std::vector<CodecParams> media_codecs_;
    };

    using OpenedCallback = std::function<void()>;
    using ClosedCallback = std::function<void()>;

public:
    MediaTrack(const Configuration& config,
               Broadcaster* broadcaster,
               TaskQueue* worker_queue);
    MediaTrack(sdp::Media description,
               Broadcaster* broadcaster,
               TaskQueue* worker_queue);
    virtual ~MediaTrack() override;

    Kind kind() const;
    const std::string mid() const;
    sdp::Media description() const;

    bool is_opened() const override;
    
    void OnOpened(OpenedCallback callback);
    void OnClosed(ClosedCallback callback);

    void OnMediaNegotiated(const sdp::Media local_media, 
                           const sdp::Media remote_media, 
                           sdp::Type remote_sdp_type);

private:
    void Open() override;
    void Close() override;
    
    void TriggerOpen();
    void TriggerClose();

public:
    // SdpBuilder
    class SdpBuilder final {
    public:
        static sdp::Media Build(const Configuration& config);
    private:
        static void AddCodecs(const Configuration& config, 
                              sdp::Media& media);
        static sdp::Media::RtpMap* AddMediaCodec(int payload_type, 
                                                 const CodecParams& cp,
                                                 sdp::Media& media);
        static void AddSsrcs(const Configuration& config, 
                             sdp::Media& media);
        static int NextPayloadType(Kind kind);
    };

protected:
    const Kind kind_;
    const sdp::Media description_;
    Broadcaster* const broadcaster_;
    TaskQueue* const worker_queue_;
    TaskQueueImpl* const signaling_queue_;

    bool is_opened_ = false;
    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::Kind kind);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::Codec codec);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, MediaTrack::FecCodec codec);

} // namespace naivertc


#endif