#include "rtc/transports/transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

Transport::Transport(std::weak_ptr<Transport> lower, std::shared_ptr<TaskQueue> task_queue) 
    : lower_(std::move(lower)), 
      is_stoped_(true),
      state_(State::DISCONNECTED) {
    task_queue_ = task_queue != nullptr ? std::move(task_queue) 
                                        : std::make_shared<TaskQueue>("Transport.default.task.queue");
}

Transport::~Transport() {}

bool Transport::is_stoped() const {
    return task_queue_->Sync<bool>([this]() -> bool {
        return is_stoped_;
    });
}

Transport::State Transport::state() const {
    return task_queue_->Sync<Transport::State>([this]() -> Transport::State {
        return state_;
    });
}

void Transport::OnStateChanged(StateChangedCallback callback) {
    task_queue_->Async([this, callback](){
        state_changed_callback_ = std::move(callback);
    });
}

// Protected methods
void Transport::UpdateState(State state) {
    task_queue_->Async([this, state](){
        if (state_ == state) 
            return;
        state_ = state;
        if (state_changed_callback_) {
            state_changed_callback_(state);
        }
    });
}

int Transport::ForwardOutgoingPacket(Packet packet) {
    return task_queue_->Sync<int>([this, packet=std::move(packet)](){
        try {
            if (auto lower = lower_.lock()) {
                return lower->Send(std::move(packet));
            }else {
                return -1;
            }
        }catch (std::exception& e) {
            PLOG_WARNING << "Failed to forward outgoing packet: " << e.what();
            return -1;
        }
    });
}

void Transport::ForwardIncomingPacket(Packet packet) {
    task_queue_->Async([this, packet=std::move(packet)](){
        try {
            if (packet_recv_callback_) {
                packet_recv_callback_(std::move(packet));
            }
        } catch (std::exception& e) {
            PLOG_WARNING << "Failed to forward incoming packet: " << e.what();
        }
    });
}

void Transport::RegisterIncoming() {
    task_queue_->Async([this](){
        if (auto lower = lower_.lock()) {
            PLOG_VERBOSE << "Registering incoming callback";
            lower->packet_recv_callback_ = std::bind(&Transport::Incoming, this, std::placeholders::_1);
        }
    });
}

void Transport::DeregisterIncoming() {
    task_queue_->Async([this](){
        if (auto lower = lower_.lock()) {
            lower->packet_recv_callback_ = nullptr;
            PLOG_VERBOSE << "Deregistered incoming callback";
        }
    });
}

} // namespace naivertc