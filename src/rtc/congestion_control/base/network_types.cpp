#include "rtc/congestion_control/base/network_types.hpp"

namespace naivertc {

// ProbeCluster
ProbeCluster::ProbeCluster(int id, 
                           int min_probes, 
                           size_t min_bytes, 
                           DataRate target_bitrate)
    : id(id),
      min_probes(min_probes),
      min_bytes(min_bytes),
      target_bitrate(target_bitrate) {}

bool ProbeCluster::IsDone() const {
    return sent_probes >= min_probes && sent_bytes >= min_bytes;
}

// PacketResult
bool PacketResult::ReceiveTimeOrder::operator()(const PacketResult& lhs,
                                                const PacketResult& rhs) {
  if (lhs.recv_time != rhs.recv_time)
    return lhs.recv_time < rhs.recv_time;
  if (lhs.sent_packet.send_time != rhs.sent_packet.send_time)
    return lhs.sent_packet.send_time < rhs.sent_packet.send_time;
  return lhs.sent_packet.packet_id < rhs.sent_packet.packet_id;
}

// TransportPacketsFeedback
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
