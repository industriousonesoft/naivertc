#include "client.hpp"

#include <plog/Log.h>

#include <string>
#include <iostream>
#include <filesystem>

namespace {

const std::string kDefaultSamplesDirRelPath = "/examples/streamer/samples/";
const std::string kDefaultH264SamplesDir = kDefaultSamplesDirRelPath + "h264/";
const std::string kDefaultOpusSamplesDir = kDefaultSamplesDirRelPath + "opus/";

} // namespace

Client::Client(boost::asio::io_context& ioc) 
    : ioc_(ioc), 
      strand_(ioc_),
      worker_queue_(std::make_unique<naivertc::TaskQueue>("worker.queue")) {}

Client::~Client() {
    ayame_channel_.reset();
}

void Client::Start() {
    if (ayame_channel_ == nullptr) {
        ayame_channel_.reset(new signaling::AyameChannel(ioc_, this));
    }
    signaling::AyameChannel::Configuration signaling_config;
    signaling_config.insecure = true;
    signaling_config.signaling_url = "wss://ayame-labo.shiguredo.jp/signaling";
    signaling_config.room_id = "industriousonesoft@ayame-labo-sample";
    signaling_config.client_id = "horseman-naive-rtc";
    signaling_config.signaling_key = "dzSU5Lz88dfZ0mVTWp51X8bPKBzfmhfdZH8D2ei3U7aNplX6";

    ayame_channel_->Connect(signaling_config);
}

void Client::Stop() {
    if (ayame_channel_) {
        ayame_channel_->Close();
    }
    if (peer_conn_) {
        peer_conn_->Close();
    }
}

// Private methods
void Client::StartVideoStream(MediaStreamSource::SampleAvailableCallback callback) {
    worker_queue_->Post([this, callback=std::move(callback)](){
        std::filesystem::path curr_dir = std::filesystem::current_path();
        auto samples_dir_path = curr_dir.string() + kDefaultH264SamplesDir;
        PLOG_VERBOSE << "samples_dir_path: " << samples_dir_path;
        if (!h264_file_stream_source_) {
            h264_file_stream_source_.reset(new H264FileStreamSource(samples_dir_path, 30, true));
            h264_file_stream_source_->OnSampleAvailable(std::move(callback));
        }
        if (!h264_file_stream_source_->IsRunning()) {
            h264_file_stream_source_->Start();
        }
    });

}

void Client::StopVideoStream() {
    worker_queue_->Post([this](){
        if (h264_file_stream_source_ && h264_file_stream_source_->IsRunning()) {
            h264_file_stream_source_->Stop();
        }
    });
}
