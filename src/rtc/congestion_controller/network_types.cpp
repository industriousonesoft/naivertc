#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {

// TransportPacketsFeedback
std::vector<PacketResult> TransportPacketsFeedback::ReceivedWithSendInfo()
    const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (fb.IsReceived()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::LostWithSendInfo() const {
  std::vector<PacketResult> res;
  for (const PacketResult& fb : packet_feedbacks) {
    if (!fb.IsReceived()) {
      res.push_back(fb);
    }
  }
  return res;
}

std::vector<PacketResult> TransportPacketsFeedback::PacketsWithFeedback()
    const {
  return packet_feedbacks;
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
