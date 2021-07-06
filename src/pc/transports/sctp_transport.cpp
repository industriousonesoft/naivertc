#include "pc/transports/sctp_transport.hpp"
#include "common/utils.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <future>

/** 
 * RFC 8831: SCTP MUST support perfoming Path MTU Discovery without relying on ICMP or ICMPv6 as 
 * specified in [RFC4821] by using probing messages specified in [RFC4820].
 * See https://tools.ietf.org/html/rfc8831#section-5
 * 
 * However, usrsctp dost not implement Path MTU Discovery, so we need to disable it for now,
 * See https://github.com/sctplab/usrsctp/issues/205
*/
#define ENABLE_PMTUD 0

// TODO: When Path MTU Discovery is supported by usrsctp, it needs to be enabled with libjuice as ICE backend on all platforms excepts MacOS on which the Don't Fragment(DF) flag can't be set:
/**
#ifndef __APPLE__
// libjuice enables Linux path MTU discovery or sets the DF flag
#define USE_PMTUD 1
#else
// Setting the DF flag is not available on Mac OS
#define USE_PMTUD 0
#endif 
*/

namespace naivertc {

// SctpTransport
SctpTransport::SctpTransport(std::shared_ptr<Transport> lower, const Config& config) 
    : Transport(lower),
    config_(std::move(config)) {
    InitUsrsctp(std::move(config));
	WeakPtrManager::SharedInstance()->Register(this);
}

SctpTransport::~SctpTransport() {
	Stop();
    usrsctp_deregister_address(this);
    WeakPtrManager::SharedInstance()->Deregister(this);
}

void SctpTransport::Start(Transport::StartedCallback callback) {
	task_queue_.Post([this, callback](){
		try {
			Transport::Start();
			this->Connect();
			if (callback) {
				callback(std::nullopt);
			}
		} catch (const std::exception& exp) {
			if (callback) {
				callback(std::make_optional<std::exception>(exp));
			}
		}
	});
}

void SctpTransport::Stop(Transport::StopedCallback callback) {
	task_queue_.Post([this, callback](){
		if (this->is_stoped()) {
			if (callback) {
				callback(std::nullopt);
			}
			return;
		}
		Transport::Stop();
		this->Shutdown();
		this->OnPacketReceived(nullptr);
		if (callback) {
			callback(std::nullopt);
		}
	});
}

void  SctpTransport::Close() {
	if (socket_) {
		usrsctp_close(socket_);
		socket_ = nullptr;
	}
}

void  SctpTransport::Connect() {
	if (socket_ == nullptr) {
		throw std::logic_error("Attempted to connect with closed sctp socket.");
	}

	PLOG_DEBUG << "SCTP connecting...";

	Transport::UpdateState(State::CONNECTING);

	struct sockaddr_conn sock_conn = {};
	sock_conn.sconn_family = AF_CONN;
	sock_conn.sconn_port = htons(config_.port);
	sock_conn.sconn_addr = this;
	sock_conn.sconn_len = sizeof(sock_conn);

	if (usrsctp_bind(socket_, reinterpret_cast<struct sockaddr *>(&sock_conn), sizeof(sock_conn)) != 0) {
		throw std::runtime_error("Failed to bind usrsctp socket, errno: " + std::to_string(errno));
	}

	// According to RFC 8841, both endpoints must initiate the SCTP association, in a
	// simultaneous-open manner, irrelevent to the SDP setup role.
	// See https://tools.ietf.org/html/rfc8841#section-9.3
	int ret = usrsctp_connect(socket_, reinterpret_cast<struct sockaddr *>(&sock_conn), sizeof(sock_conn));
	if ( ret != 0 || ret != EINPROGRESS) {
		throw std::runtime_error("Failed to connect usrsctp socket, errno: " + std::to_string(errno));
	}

}

void SctpTransport::Shutdown() {
	if (!socket_) return;

	PLOG_DEBUG << "SCTP shutdown.";

	if (usrsctp_shutdown(socket_, SHUT_RDWR) != 0 && errno != ENOTCONN /* ENOTCONN: not connection error */ ) {
		PLOG_WARNING << "SCTP shutdown failed, errno= " << errno;
	}

	Close();
	Transport::UpdateState(State::DISCONNECTED);
	PLOG_INFO << "SCTP disconnected.";

}

// Send
bool SctpTransport::Flush() {
	if (!task_queue_.is_in_current_queue()) {
		std::promise<bool> promise;
		auto future = promise.get_future();
		task_queue_.Post([this, &promise](){
			promise.set_value(this->Flush());
		});
		return future.get();
	}
	try {
		TrySendQueue();
		return true;
	} catch(const std::exception& exp) {
		PLOG_WARNING << "Faild to flush messages: " << exp.what();
		return false;
	}
}

void SctpTransport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
	task_queue_.Post([=](){
		if (!utils::instanceof<SctpPacket>(packet.get())) {
			PLOG_WARNING << "Try to send a non-sctp message.";
			if (callback) {
				callback(false);
			}
			return;
		}

		if (!packet) {
			auto bRet = this->TrySendQueue();
			if (callback) {
				callback(bRet);
			}
			return;
		}
		auto message = std::dynamic_pointer_cast<SctpPacket>(packet);
		// Flush the queue, and if nothing is pending, try to send directly
		if (this->TrySendQueue() && this->TrySendMessage(message)) {
			if (callback) {
				callback(true);
			}
			return;
		}

		// enqueue
		this->send_message_queue_.push(message);
		this->UpdateBufferedAmount(message->stream_id(), ptrdiff_t(message->message_size()));
		if (callback) {
			callback(false);
		}
	});
}

bool SctpTransport::TrySendQueue() {
	while (auto next = send_message_queue_.back()) {
		auto message = std::move(next);
		if (!TrySendMessage(message)) {
			return false;
		}
		send_message_queue_.pop();
		UpdateBufferedAmount(message->stream_id(), -ptrdiff_t(message->message_size()));
	}
	return false;
}

bool SctpTransport::TrySendMessage(std::shared_ptr<SctpPacket> message) {
	if (!socket_ || state() == State::CONNECTED) {
		return false;
	}
	PayloadId ppid;
	switch (message->type()) {
	case SctpPacket::Type::STRING:
		ppid = message->is_empty() ? PayloadId::PPID_STRING_EMPTY : PayloadId::PPID_STRING;
		break;
	case SctpPacket::Type::BINARY:
		ppid = message->is_empty() ? PayloadId::PPID_BINARY_EMPTY : PayloadId::PPID_BINARY;
		break;
	case SctpPacket::Type::CONTROL:
		ppid = PayloadId::PPID_CONTROL;	
		break;
	case SctpPacket::Type::RESET:
		ResetStream(message->stream_id());
		return true;
	default:
		// Ignore
		return true;
	}

	// TODO: Implement SCTP ndata specification draft when supported everywhere
	// See https://tools.ietf.org/html/draft-ietf-tsvwg-sctp-ndata-08

	const auto reliability = message->reliability() ? *message->reliability() : SctpPacket::Reliability();
	
	struct sctp_sendv_spa spa = {};
	
	// set sndinfo
	spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
	spa.sendv_sndinfo.snd_sid = message->stream_id();
	spa.sendv_sndinfo.snd_ppid = htonl(uint32_t(ppid));
	spa.sendv_sndinfo.snd_flags |= SCTP_EOR;

	// set prinfo
	spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
	if (reliability.ordered == false) {
		spa.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
	}

	switch (reliability.polity) {
	case SctpPacket::Reliability::Policy::RTX: {
		spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
		spa.sendv_prinfo.pr_value = utils::numeric::to_uint32(std::get<int>(reliability.rexmit));
		break;
	}
	case SctpPacket::Reliability::Policy::TTL: {
		spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
		spa.sendv_prinfo.pr_value = utils::numeric::to_uint32(std::get<std::chrono::milliseconds>(reliability.rexmit).count());
		break;
	}
	default:
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_NONE;
		break;
	}

	ssize_t ret;
	if (!message->is_empty()) {
		ret = usrsctp_sendv(socket_, message->data(), message->size(), nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
	}else {
		const char zero = 0;
		ret = usrsctp_sendv(socket_, &zero, 1, nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
	}

	if (ret < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			PLOG_WARNING << "SCTP sending not possible.";
			return false;
		}

		PLOG_ERROR << "SCTP sending failed, errno: " << errno;
		throw std::runtime_error("Faild to send SCTP message, errno: " + std::to_string(errno));
	}

	PLOG_VERBOSE << "SCTP send size: " << message->size();

	if (message->type() == SctpPacket::Type::STRING || message->type() == SctpPacket::Type::BINARY) {
		bytes_sent_ += message->size();
	}

	return true;
}

void SctpTransport::UpdateBufferedAmount(StreamId stream_id, ptrdiff_t delta) {
	// Find the pair for stream_id, or create a new pair
	auto it = buffered_amount_.insert(std::make_pair(stream_id, 0)).first;
	size_t amount = size_t(std::max(ptrdiff_t(it->second) + delta, ptrdiff_t(0)));
	if (amount == 0) {
		buffered_amount_.erase(it);
	}else {
		it->second = amount;
	}
	SignalBufferedAmountChanged(stream_id, amount);
}

void SctpTransport::DoRecv() {
	try {
		while (true) {
			auto state = State();
			if (state == State::DISCONNECTED || state == State::FAILED) {
				break;
			}
			socklen_t from_len = 0;
			struct sctp_rcvinfo info = {};
			socklen_t info_len = sizeof(info);
			unsigned int info_type = 0;
			int flags = 0;
			ssize_t len = usrsctp_recvv(socket_, buffer_, buffer_size_, nullptr, &from_len, &info, &info_len, &info_type, &flags);

			if (len < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ECONNRESET) {
					break;
				}else {
					throw std::runtime_error("SCTP recv failed. errno: " + std::to_string(errno));
				}
			}

			PLOG_VERBOSE << "SCTP recv len: " << len;

			// SCTP_FRAGMENT_INTERLEAVE does not seem to work as expected for message > 64KB
			// therefore partinal notifications and messages need to be handled separately.
			if (flags & MSG_NOTIFICATION) {
				notification_data_fragments_.insert(notification_data_fragments_.end(), buffer_, buffer_ + len);
				// Notification is complete, so process it.
				if (flags & MSG_EOR) {
					auto notification = reinterpret_cast<union sctp_notification *>(notification_data_fragments_.data());
					ProcessNotification(notification, notification_data_fragments_.size());
					notification_data_fragments_.clear();
				}
			}else {
				message_data_fragments_.insert(message_data_fragments_.end(), buffer_, buffer_ + len);
				if (flags & MSG_EOR) {
					if (info_type != SCTP_RECVV_RCVINFO) {
						throw std::runtime_error("Missing SCTP recv info.");
					}
					ProcessMessage(std::move(message_data_fragments_), info.rcv_sid, PayloadId(ntohl(info.rcv_ppid)));
					message_data_fragments_.clear();
				}
			}
		}
	} catch (const std::exception& exp) {
		PLOG_WARNING << exp.what();
	}
}

void SctpTransport::DoFlush() {
	try {
		TrySendQueue();
	} catch(const std::exception& exp) {
		PLOG_WARNING << exp.what();
	}
}

void SctpTransport::CloseStream(StreamId stream_id) {
	const std::byte stream_close_message{0x0};
	auto message = SctpPacket::Create(&stream_close_message, 1, SctpPacket::Type::RESET, stream_id);
	Send(message);
}

void SctpTransport::ResetStream(StreamId stream_id) {
	if (socket_ == nullptr || state() == State::CONNECTED) {
		return;
	}

	PLOG_DEBUG << "SCTP resetting stream: " << stream_id;

	using srs_t = struct sctp_reset_streams;
	const size_t len = sizeof(srs_t) + sizeof(StreamId);
	std::byte buffer[len] = {};
	srs_t& srs = *reinterpret_cast<srs_t *>(buffer);
	srs.srs_flags = SCTP_STREAM_RESET_OUTGOING;
	srs.srs_number_streams = 1;
	srs.srs_stream_list[0] = stream_id;

	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_RESET_STREAMS, &srs, len) != 0) {
		if (errno == EINVAL) {
			PLOG_DEBUG << "SCTP stream: " << stream_id << " already reset.";
		}else {
			PLOG_WARNING << "SCTP reset stream " << stream_id << " failed, errno: " << std::to_string(errno);
		}
	}
}

void SctpTransport::ProcessNotification(const union sctp_notification* notification, size_t len) {
	if (len != sizeof(notification->sn_header.sn_length)) {
		PLOG_WARNING << "Invalid SCTP notification length";
		return;
	}

	auto type = notification->sn_header.sn_type;

	PLOG_VERBOSE << "Processing notification type: " << type;

	switch (type) {
	case SCTP_ASSOC_CHANGE: {
		const struct sctp_assoc_change &assoc_change = notification->sn_assoc_change;
		if (assoc_change.sac_state == SCTP_COMM_UP) {
			PLOG_INFO << "SCTP connected.";
			UpdateState(State::CONNECTED);
		}else {
			if (State() == State::CONNECTING) {
				PLOG_ERROR << "SCTP connection failed.";
				UpdateState(State::FAILED);
			}else {
				PLOG_INFO << "SCTP disconnected.";
				UpdateState(State::DISCONNECTED);
			}
			// SCTP 连接失败意味着不可能发送消息
			waiting_for_sending_condition_.notify_all();
		}
		break;
	}
	case SCTP_SENDER_DRY_EVENT: {
		PLOG_VERBOSE << "SCTP dry event.";
		// It should not be necessary since the sand callback should have been called already,
		// but to be sure, let's try to send now.
		Flush();
		break;
	}

	case SCTP_STREAM_RESET_EVENT: {
		const struct sctp_stream_reset_event& reset_event = notification->sn_strreset_event;
		const int count = (reset_event.strreset_length - sizeof(reset_event)) / sizeof(uint16_t);
		const uint16_t flags = reset_event.strreset_flags;

		IF_PLOG(plog::verbose) {
			std::ostringstream desc;
			desc << "flags=";
			if (flags & SCTP_STREAM_RESET_OUTGOING_SSN && flags & SCTP_STREAM_RESET_INCOMING_SSN)
				desc << "outgoing|incoming";
			else if (flags & SCTP_STREAM_RESET_OUTGOING_SSN)
				desc << "outgoing";
			else if (flags & SCTP_STREAM_RESET_INCOMING_SSN)
				desc << "incoming";
			else
				desc << "0";

			desc << ", streams=[";
			for (int i = 0; i < count; ++i) {
				StreamId stream_id = reset_event.strreset_stream_list[i];
				desc << (i != 0 ? "," : "") << stream_id;
			}
			desc << "]";

			PLOG_VERBOSE << "SCTP reset event, " << desc.str();
		}

		if (flags & SCTP_STREAM_RESET_OUTGOING_SSN) {
			for (int i = 0; i < count; ++i) {
				StreamId stream_id = reset_event.strreset_stream_list[i];
				CloseStream(stream_id);
			}
		}

		if (flags & SCTP_STREAM_RESET_INCOMING_SSN) {
			const std::byte data_channel_close_message{0x04};
			for (int i = 0; i < count; ++i) {
				StreamId stream_id = reset_event.strreset_stream_list[i];
				auto message = SctpPacket::Create(&data_channel_close_message, 1, SctpPacket::Type::CONTROL, stream_id);
				HandleIncomingPacket(message);
			}
		}

		break;
	}
	default:
		break;
	}
}

void SctpTransport::ProcessMessage(std::vector<std::byte>&& data, StreamId stream_id, SctpTransport::PayloadId payload_id) {
	// RFC 8831: The usage of the PPIDs "WebRTC String Partial" and "WebRTC Binary Partial" is
	// deprecated. They were used for a PPID-based fragmentation and reassembly of user messages
	// belonging to reliable and ordered data channels.
	// See https://tools.ietf.org/html/rfc8831#section-6.6
	// We handle those PPIDs at reception for compatibility reasons but shall never send them.
	switch (payload_id) {
	case PayloadId::PPID_CONTROL: {
		auto message = SctpPacket::Create(std::move(data), SctpPacket::Type::CONTROL, stream_id);
		HandleIncomingPacket(message);
		break;
	}
	case PayloadId::PPID_STRING_PARTIAL: 
		string_data_fragments_.insert(string_data_fragments_.end(), data.begin(), data.end());
		break;
	case PayloadId::PPID_STRING: {
		if (string_data_fragments_.empty()) {
			bytes_recv_ += data.size();
			auto message = SctpPacket::Create(std::move(data), SctpPacket::Type::STRING, stream_id);
			HandleIncomingPacket(message);
		}else {
			string_data_fragments_.insert(string_data_fragments_.end(), data.begin(), data.end());
			bytes_recv_ += data.size();
			auto message = SctpPacket::Create(std::move(string_data_fragments_), SctpPacket::Type::STRING, stream_id);
			HandleIncomingPacket(message);
			string_data_fragments_.clear();
		}
		break;
	}
	case PayloadId::PPID_STRING_EMPTY: {
		auto message = SctpPacket::Create(std::move(string_data_fragments_), SctpPacket::Type::STRING, stream_id);
		HandleIncomingPacket(message);
		string_data_fragments_.clear();
		break;
	}
	case PayloadId::PPID_BINARY_PARTIAL: 
		binary_data_fragments_.insert(binary_data_fragments_.end(), data.begin(), data.end());
		break;
	case PayloadId::PPID_BINARY: {
		if (binary_data_fragments_.empty()) {
			bytes_recv_ += data.size();
			auto message = SctpPacket::Create(std::move(data), SctpPacket::Type::BINARY, stream_id);
			HandleIncomingPacket(message);
		}else {
			binary_data_fragments_.insert(binary_data_fragments_.end(), data.begin(), data.end());
			bytes_recv_ += data.size();
			auto message = SctpPacket::Create(std::move(binary_data_fragments_), SctpPacket::Type::BINARY, stream_id);
			HandleIncomingPacket(message);
			binary_data_fragments_.clear();
		}
		break;
	}
	case PayloadId::PPID_BINARY_EMPTY: {
		auto message = SctpPacket::Create(std::move(binary_data_fragments_), SctpPacket::Type::BINARY, stream_id);
		HandleIncomingPacket(message);
		binary_data_fragments_.clear();
		break;
	}
	default:
		PLOG_VERBOSE << "Unknown PPID: " << uint32_t(payload_id);
		break;
	}
}

// SCTP callback methods
void SctpTransport::OnSCTPRecvDataIsReady() {
	task_queue_.Post([this](){
		if (this->socket_ == nullptr)
			return;

		int events = usrsctp_get_events(this->socket_);

		if (events > 0 && SCTP_EVENT_READ) {
			this->DoRecv();
		}

		if (events > 0 && SCTP_EVENT_WRITE) {
			this->DoFlush();
		}
	});
}
    
int SctpTransport::OnSCTPSendDataIsReady(const void* data, size_t len, uint8_t tos, uint8_t set_df) {
	std::promise<bool> promise;
	auto future = promise.get_future();
	auto packet = Packet::Create(static_cast<const char*>(data), len);
	Outgoing(std::move(packet), [this, &promise](bool success){
		promise.set_value(success);
		// Reset the sent flag and ready to handle the incoming message
		this->has_sent_once_ = true;
		this->waiting_for_sending_condition_.notify_all();
	});
	return future.get() ? 0 : -1;
}

// Override methods
void SctpTransport::Incoming(std::shared_ptr<Packet> in_packet) {
	// There could be a race condition here where we receive the remote INIT before the local one is
	// sent, which would result in the connection being aborted. Therefore, we need to wait for data
	// to be sent on our side (i.e. the local INIT) before proceeding.
	if (!has_sent_once_) {
		std::unique_lock lock(waiting_for_sending_mutex_);
		waiting_for_sending_condition_.wait(lock, [&]() {
			// return false if the waiting should be continued.
			return has_sent_once_.load();
		});
	}

	if (!in_packet) {
		Transport::UpdateState(State::DISCONNECTED);
		// To notify the recv callback
		HandleIncomingPacket(nullptr);
		return;
	}

	PLOG_VERBOSE << "Incoming size: " << in_packet->size();

	// 触发回调函数sctp_recv_data_ready_cb？
	usrsctp_conninput(this, in_packet->data(), in_packet->size(), 0);
}

void SctpTransport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
	task_queue_.Post([=](){
		// Set recommended medium-priority DSCP value
		// See https://tools.ietf.org/html/draft-ietf-tsvwg-rtcweb-qos-18
		out_packet->set_dscp(10); // AF11: Assured Forwarding class 1, low drop probability
		return Transport::Outgoing(std::move(out_packet), callback);
	});
}

}