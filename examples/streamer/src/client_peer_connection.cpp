#include "client.hpp"

#include <plog/Log.h>

#include <iostream>

#define HAS_MEDIA 1

template <class T> std::weak_ptr<T> make_weak_ptr(std::shared_ptr<T> ptr) { return ptr; }

// Private methods
void Client::CreatePeerConnection(const RtcConfiguration& rtc_config) {
    if (peer_conn_) {
        peer_conn_->Close();
        peer_conn_.reset();
    }

    peer_conn_ = PeerConnection::Create(rtc_config);

    peer_conn_->OnConnectionStateChanged([](PeerConnection::ConnectionState new_state){
        PLOG_INFO << "Peer connection state:" << new_state;
    });

    peer_conn_->OnIceGatheringStateChanged([](PeerConnection::GatheringState new_state) {
        PLOG_INFO << "Peer gathering state: " << new_state;
    });

    peer_conn_->OnIceCandidateGathered([this](const sdp::Candidate& candidate){
        auto mid = candidate.mid();
        auto sdp = std::string(candidate);
        PLOG_INFO << "Local candidate => mid: " << mid << " sdp: " << sdp;
        ioc_.post(strand_.wrap([this, mid = std::move(mid), sdp = std::move(sdp)](){
            this->SendLocalCandidate(mid, sdp);
        }));
    });

#if HAS_MEDIA
    // TODO: Generate a random 16-char and case-insensitive string, e.g.; TjtznXLCNH7nbRw
    std::string cname = "naivertc-media-cname";
    // TODO: Generate a random 36-char and case-insensitive string, e.g.; h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    std::string media_stream_id = "naivertc-media-stream-id";

    // Local audio track
    AddAudioTrack(cname, media_stream_id);

    // Local video track
    AddVideoTrack(cname, media_stream_id);
#endif

    // Data channel
    AddDataChannel();

    // Incoming data channel from remote peer
    peer_conn_->OnRemoteDataChannelReceived([](std::shared_ptr<DataChannel> data_channel){
        PLOG_INFO << "Incoming data channel:" << data_channel->stream_id();
    });

    // Incoming media track from remote peer
    peer_conn_->OnRemoteMediaTrackReceived([](std::shared_ptr<MediaTrack> media_track){
        PLOG_INFO << "Incoming media track:" << media_track->mid();
    });

}

// AddAudioTrack
void Client::AddAudioTrack(std::string cname, std::string stream_id) {
    MediaTrack::Configuration audio_track_config(MediaTrack::Kind::AUDIO, "1");
    audio_track_config.direction = MediaTrack::Direction::SEND_ONLY;
    audio_track_config.AddCodec(MediaTrack::Codec::OPUS);

    audio_track_config.cname.emplace(cname);
    audio_track_config.msid.emplace(stream_id);
    audio_track_config.track_id.emplace("audio-track-id-1");
    
    audio_track_ = peer_conn_->AddAudioTrack(audio_track_config);
    audio_track_->OnOpened([](){
        PLOG_INFO << "Local audio track is opened.";
    });
    audio_track_->OnClosed([](){
        PLOG_INFO << "Local audio track is closed.";
    });
}

// AddVideoTrack
void Client::AddVideoTrack(std::string cname, std::string stream_id) {
    MediaTrack::Configuration video_track_config(MediaTrack::Kind::VIDEO, "2");
    video_track_config.direction = MediaTrack::Direction::SEND_ONLY;
    video_track_config.rtx_enabled = true;
    video_track_config.nack_enabled = true;
    video_track_config.congestion_control = MediaTrack::CongestionControl::TRANSPORT_CC;
    video_track_config.fec_codec.emplace(MediaTrack::FecCodec::ULP_FEC);
    video_track_config.AddCodec(MediaTrack::Codec::H264);

    video_track_config.cname.emplace(cname);
    video_track_config.msid.emplace(stream_id);
    video_track_config.track_id.emplace("video-track-id-1");

    video_track_ = peer_conn_->AddVideoTrack(video_track_config);
    video_track_->OnOpened([this](){
        PLOG_INFO << "Local video track is opened.";
        this->StartVideoStream([this](H264FileStreamSource::Sample sample, bool is_key_frame, int64_t capture_time_ms){
            video::EncodedFrame encoded_frame(std::move(sample));
            encoded_frame.set_width(1280);
            encoded_frame.set_height(720);
            encoded_frame.set_frame_type(is_key_frame ? video::FrameType::KEY : video::FrameType::DELTA);
            encoded_frame.set_codec_type(video::CodecType::H264);
            encoded_frame.set_timestamp(static_cast<uint32_t>(capture_time_ms * 90));
            encoded_frame.set_capture_time_ms(capture_time_ms);
            encoded_frame.set_retransmission_allowed(true);
            video_track_->Send(std::move(encoded_frame));
        });
    });
    video_track_->OnClosed([this](){
        PLOG_INFO << "Local video track is closed.";
        this->StopVideoStream();
    });
}

// AddDataChannel
void Client::AddDataChannel() {
    DataChannel::Init data_channel_init("naivertc-chat-data-channel");
    data_channel_ = peer_conn_->AddDataChannel(data_channel_init);

    data_channel_->OnOpened([weak_dc=make_weak_ptr(data_channel_)](){
        if (auto dc = weak_dc.lock()) {
            PLOG_INFO << "OnOpened : " << weak_dc.lock()->label();
        }
    });

    data_channel_->OnClosed([weak_dc=make_weak_ptr(data_channel_)](){
        if (auto dc = weak_dc.lock()) {
            PLOG_INFO << "OnClosed : " << weak_dc.lock()->label();
        }
    });

    data_channel_->OnMessageReceived([weak_dc=make_weak_ptr(data_channel_)](const std::string text){
        PLOG_VERBOSE << "OnTextMessageReceived : " << text;
        if (auto dc = weak_dc.lock()) {
            auto res = "Hi, " + text;
            PLOG_VERBOSE << "Response: " << res;
            dc->Send(res);
        }
    });

    data_channel_->OnMessageReceived([](const uint8_t* in_data, size_t in_size){
        PLOG_VERBOSE << "OnBinaryMessageReceived : " << in_size;
    });

    data_channel_->OnBufferedAmountChanged([](uint64_t previous_amount){
        PLOG_VERBOSE << "OnBufferedAmountChanged : " << previous_amount;
    });
}

void Client::SendLocalSDP(const std::string sdp, bool is_offer) {
    if (signaling_client_) {
        signaling_client_->SendSDP(sdp, is_offer);
    }
}

void Client::SendLocalCandidate(const std::string mid, const std::string sdp) {
    if (signaling_client_) {
        signaling_client_->SendCandidate(mid, 0, sdp);
    }
}