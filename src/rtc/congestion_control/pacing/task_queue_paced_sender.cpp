#include "rtc/congestion_control/pacing/task_queue_paced_sender.hpp"

namespace naivertc {

TaskQueuePacedSender::TaskQueuePacedSender(const Configuration& config, 
                                           TimeDelta max_hold_back_window,
                                           int max_hold_window_in_packets) 
    : clock_(config.clock),
      max_hold_back_window_(max_hold_back_window),
      max_hold_window_in_packets_(max_hold_window_in_packets),
      pacing_controller_(config),
      packet_size_(/*alpha=*/0.95),
      task_queue_("TaskQueuePacedSender.task.queue") {

}
    
TaskQueuePacedSender::~TaskQueuePacedSender() {

}

void TaskQueuePacedSender::EnqueuePackets(std::vector<RtpPacketToSend> packets) {

}
    
} // namespace naivertc
