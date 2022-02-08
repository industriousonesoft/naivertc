#include "stream/h264_file_stream_source.hpp"

H264FileStreamSource::H264FileStreamSource(const std::string directory, int fps, bool loop) 
    : MediaFileStreamSource(directory, ".h264", fps, loop) {}

H264FileStreamSource::~H264FileStreamSource() = default;

void H264FileStreamSource::GenerateSample(std::ifstream& source, int64_t now_ms) {
    bool is_key_frame = false;
    Sample sample;
    sample.assign((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    size_t offset = 0;
    while (offset + 4 < sample.size()) {
       auto nalu_len_ptr = (uint32_t*)(sample.data() + offset);
       uint32_t nalu_len = ntohl(*nalu_len_ptr);
       size_t nalu_start_offset = offset + 4;
       size_t nalu_end_offset = nalu_start_offset + nalu_len;
       if (nalu_end_offset >= sample.size()) {
           break;
       }
       uint8_t nalu_type = sample.data()[nalu_end_offset] & 0x1F;
       if (nalu_type == 7 /* SPS */ || nalu_type == 8 /* PPS */  || nalu_type == 5 /* IDR */ ) {
           is_key_frame = true;;
           break;
       }
       offset = nalu_start_offset;
    }
    if (sample_callback_) {
        sample_callback_(std::move(sample), is_key_frame, now_ms);
    }
}