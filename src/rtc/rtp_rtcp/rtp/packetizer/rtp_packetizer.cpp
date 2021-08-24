#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer.hpp"

#include <plog/Log.h>

namespace naivertc {

std::vector<size_t> RtpPacketizer::SplitAboutEqually(size_t payload_size, const PayloadSizeLimits& limits) {
    std::vector<size_t> result;
    if (payload_size == 0) {
        return result;
    }

    if (limits.first_packet_reduction_size < 0 || limits.last_packet_reduction_size < 0) {
        PLOG_WARNING << "First or last packet larger than normal size are unsupported.";
        return result;
    }

    // Can packetize into a single packet
    if (limits.max_payload_size >= limits.single_packet_reduction_size + payload_size) {
        result.push_back(payload_size);
        return result;
    }

    // Capacity is not enough to put a single byte into one of the packets.
    if (limits.max_payload_size - limits.first_packet_reduction_size < 1 ||
        limits.max_payload_size - limits.last_packet_reduction_size < 1) {
        return result;
    }

    // First and last packet of the frame can be smaller. Pretend that it's
    // the same size, but we must write more payload to it.
    // Assume frame fits in single packet if packet has extra space for sum
    // of first and last packets reductions.
    size_t total_size = payload_size + limits.first_packet_reduction_size + limits.last_packet_reduction_size;

    // Interger divisions with rounding up.
    size_t num_of_packets = (total_size + (limits.max_payload_size - 1 /* the largest padding */)) / limits.max_payload_size;
    if (num_of_packets == 1) {
        // Single packet is a special case handled above.
        num_of_packets += 1;
    }

    if (payload_size < num_of_packets) {
        // Edge case where limits force to have more packets than there are payload
        // bytes. This may happen when there is single byte of payload that can't be
        // put into single packet if
        // first_packet_reduction + last_packet_reduction >= max_payload_len.
        return result;
    }

    size_t bytes_per_packet = total_size / num_of_packets;
    size_t num_larger_packets = total_size % num_of_packets;
    size_t remaining_size = payload_size;

    size_t num_of_packets_left = num_of_packets;
    result.reserve(num_of_packets_left);
    bool first_packet = true;
    while (remaining_size > 0) {
        // Last num_larger_packets are 1 byte wider than the rest. Increase
        // per-packet payload size when needed.
        if (num_of_packets_left == num_larger_packets)
            ++bytes_per_packet;
        size_t current_packet_size = bytes_per_packet;
        if (first_packet) {
            if (current_packet_size > limits.first_packet_reduction_size + 1)
                current_packet_size -= limits.first_packet_reduction_size;
            else
                current_packet_size = 1;
            }
            if (current_packet_size > remaining_size) {
                current_packet_size = remaining_size;
            }
            // This is not the last packet in the whole payload, but there's no data
            // left for the last packet. Leave at least one byte for the last packet.
            if (num_of_packets_left == 2 && current_packet_size == remaining_size) {
                --current_packet_size;
        }
        result.push_back(current_packet_size);

        remaining_size -= current_packet_size;
        --num_of_packets_left;
        first_packet = false;
    }

    return result;
}
    
} // namespace naivertc
