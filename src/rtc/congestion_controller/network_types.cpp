#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {

// TransportPacketsFeedback

bool PacketResult::ReceiveTimeOrder::operator()(const PacketResult& lhs,
                                                const PacketResult& rhs) {
  if (lhs.recv_time != rhs.recv_time)
    return lhs.recv_time < rhs.recv_time;
  if (lhs.sent_packet.send_time != rhs.sent_packet.send_time)
    return lhs.sent_packet.send_time < rhs.sent_packet.send_time;
  return lhs.sent_packet.packet_id < rhs.sent_packet.packet_id;
}

std::vector<PacketResult> TransportPacketsFeedback::ReceivedPackets() const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.IsReceived()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::LostPackets() const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (!fb.IsReceived()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::SortedByReceiveTime()
    const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.IsReceived()) {
      res.push_back(fb);
    }
  }
  std::sort(res.begin(), res.end(), PacketResult::ReceiveTimeOrder());
  return res;
}
    
} // namespace naivertc
