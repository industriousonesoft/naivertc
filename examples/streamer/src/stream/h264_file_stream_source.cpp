#include "stream/h264_file_stream_source.hpp"

H264FileStreamSource::H264FileStreamSource(const std::string directory, int fps, bool loop) 
    : MediaFileStreamSource(directory, ".h264", fps, loop) {}

H264FileStreamSource::~H264FileStreamSource() = default;

H264FileStreamSource::Sample H264FileStreamSource::CreateSample(std::ifstream& source) {
    Frame frame;
    frame.assign((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    size_t offset = 0;
    while (offset + 4 < frame.size()) {
       auto nalu_len_ptr = (uint32_t*)(frame.data() + offset);
       uint32_t nalu_len = ntohl(*nalu_len_ptr);
       size_t nalu_start_offset = offset + 4;
       size_t nalu_end_offset = nalu_start_offset + nalu_len;
       if (nalu_end_offset >= frame.size()) {
           break;
       }
       uint8_t nalu_type = frame.data()[nalu_end_offset] & 0x1F;
       if (nalu_type == 7 /* SPS */ || nalu_type == 8 /* PPS */  || nalu_type == 5 /* IDR */ ) {
           frame.set_is_key_frame(true);
           break;
       }
    }
    return std::move(frame);
}