#include "rtc/transports/sctp_transport.hpp"
#include "common/utils_numeric.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

// SctpTransport
SctpTransport::SctpTransport(Configuration config, Transport* lower, TaskQueue* task_queue) 
    : Transport(lower, task_queue),
      config_(std::move(config)),
	  // AF11: Assured Forwarding class 1, low drop probability
	  packet_options_(DSCP::DSCP_AF11, PacketKind::TEXT) {
	task_queue_->Async([this](){
		WeakPtrManager::SharedInstance()->Register(this);
	});
}

SctpTransport::~SctpTransport() {
	Close();
	task_queue_->Async([this](){
		usrsctp_deregister_address(this);
    	WeakPtrManager::SharedInstance()->Deregister(this);
	});
}

void SctpTransport::OnSctpMessageReceived(SctpMessageReceivedCallback callback) {
	task_queue_->Async([this, callback=std::move(callback)](){
		sctp_message_received_callback_ = std::move(callback);
	});
}

void SctpTransport::OnReadyToSend(ReadyToSendCallback callback) {
	task_queue_->Async([this, callback=std::move(callback)](){
		ready_to_send_callback_ = std::move(callback);
	});
}

bool SctpTransport::Start() {
	return task_queue_->Sync<bool>([this](){
		try {
			if (is_stoped_) {
				Reset();
				RegisterIncoming();
				Connect();
				is_stoped_ = false;
			}
			return true;
		}catch(std::exception& e) {
			PLOG_WARNING << e.what();
			UpdateState(State::FAILED);
			return false;
		}
	});
}

bool SctpTransport::Stop() {
	return task_queue_->Sync<bool>([this](){
		try {
			if (!is_stoped_) {
				DeregisterIncoming();
				// Shutdwon SCTP connection
				Shutdown();
				is_stoped_ = true;
				// TODO: Reset callback
			}
			return true;
		}catch(std::exception& e) {
			PLOG_WARNING << e.what();
			UpdateState(State::FAILED);
			return false;
		}
	});
}

void SctpTransport::CloseStream(uint16_t stream_id) {
	task_queue_->Async([this, stream_id](){
		ResetStream(stream_id);
	});
}

bool SctpTransport::Send(SctpMessageToSend message) {
	return task_queue_->Sync<bool>([this, message=std::move(message)](){
		return SendInternal(std::move(message));
	});
}

// Private method
void SctpTransport::Close() {
	RTC_RUN_ON(task_queue_);
	if (socket_) {
		usrsctp_close(socket_);
		socket_ = nullptr;
	}
}

void SctpTransport::Reset() {
	RTC_RUN_ON(task_queue_);
	bytes_sent_ = 0;
    bytes_recv_ = 0;
	
	notification_data_fragments_.clear();
    message_data_fragments_.clear();

    string_data_fragments_.clear();
    binary_data_fragments_.clear();

	partial_outgoing_packet_.reset();
	std::queue<CopyOnWriteBuffer>().swap(pending_incoming_packets_);
}

void SctpTransport::Connect() {
	RTC_RUN_ON(task_queue_);
	if (socket_ != nullptr) {
		PLOG_VERBOSE << "SCTP is already connected.";
		return;
	}

	PLOG_DEBUG << "SCTP connecting";

	UpdateState(State::CONNECTING);

	OpenSctpSocket();

	struct sockaddr_conn sock_conn = {};
	sock_conn.sconn_family = AF_CONN;
	sock_conn.sconn_port = htons(config_.port);
	sock_conn.sconn_addr = this;
#ifdef HAVE_SCONN_LEN // Defined in usrsctp lib
	sock_conn.sconn_len = sizeof(sock_conn);
#endif

	if (usrsctp_bind(socket_, reinterpret_cast<struct sockaddr *>(&sock_conn), sizeof(sock_conn)) != 0) {
		throw std::runtime_error("Failed to bind usrsctp socket, errno: " + std::to_string(errno));
	}

	// According to RFC 8841, both endpoints must initiate the SCTP association, in a
	// simultaneous-open manner, irrelevent to the SDP setup role.
	// See https://tools.ietf.org/html/rfc8841#section-9.3
	int ret = usrsctp_connect(socket_, reinterpret_cast<struct sockaddr *>(&sock_conn), sizeof(sock_conn));
	if (ret < 0 && errno != EINPROGRESS) {
		auto err_str = std::to_string(errno);
		PLOG_ERROR << "usrsctp connect errno: " << err_str;
		throw std::runtime_error("Failed to connect usrsctp socket, errno: " + err_str);
	}

	// Since this is a fresh SCTP association, we'll always start out with empty
    // queues, so "ReadyToSendData" should be true.
	ReadyToSend();
}

void SctpTransport::Shutdown() {
	RTC_RUN_ON(task_queue_);
	if (!socket_) return;

	PLOG_DEBUG << "SCTP shutdown.";

	if (usrsctp_shutdown(socket_, SHUT_RDWR) != 0 && errno != ENOTCONN /* ENOTCONN: not connection error */ ) {
		PLOG_WARNING << "SCTP shutdown failed, errno= " << errno;
	}
	Close();
	UpdateState(State::DISCONNECTED);
}

// Send
bool SctpTransport::SendInternal(SctpMessageToSend message) {
	RTC_RUN_ON(task_queue_);
	if (partial_outgoing_packet_.has_value()) {
		ready_to_send_ = false;
		return false;
	}
	if (TrySend(message)) {
		if (message.available_payload_size() > 0) {
			partial_outgoing_packet_.emplace(std::move(message));
		}
		return true;	
	}
	return false;
}

bool SctpTransport::FlushPendingMessage() {
	RTC_RUN_ON(task_queue_);
	if (partial_outgoing_packet_.has_value()) {
		auto& message = partial_outgoing_packet_.value();
		if (TrySend(message)) {
			if (message.available_payload_size() == 0) {
				partial_outgoing_packet_.reset();
				return true;
			} else {
				return false;
			}
		}
	}
	return true;
}

bool SctpTransport::TrySend(SctpMessageToSend& message) {
	RTC_RUN_ON(task_queue_);
	if (!socket_ || state_ != State::CONNECTED) {
		return false;
	}
	const size_t available_payload_size = message.available_payload_size();
	PayloadId ppid;
	switch (message.type()) {
	case SctpMessage::Type::STRING:
		ppid = available_payload_size == 0 ? PayloadId::PPID_STRING_EMPTY : PayloadId::PPID_STRING;
		break;
	case SctpMessage::Type::BINARY:
		ppid = available_payload_size == 0 ? PayloadId::PPID_BINARY_EMPTY : PayloadId::PPID_BINARY;
		break;
	case SctpMessage::Type::CONTROL:
		ppid = PayloadId::PPID_CONTROL;	
		break;
	default:
		// Ignore
		return false;
	}

	// TODO: Implement SCTP ndata specification draft when supported everywhere
	// See https://tools.ietf.org/html/draft-ietf-tsvwg-sctp-ndata-08

	struct sctp_sendv_spa spa = {0};
	// set sndinfo
	spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
	spa.sendv_sndinfo.snd_sid = message.stream_id();
	spa.sendv_sndinfo.snd_ppid = htonl(uint32_t(ppid));
	// Explicitly marking the EOR flag turns the usrsctp_sendv call below into a
	// non atomic operation. This means that the sctp lib might only accept the
	// message partially. This is done in order to improve throughput, so that we
	// don't have to wait for an empty buffer to send the max message length, for
	// example.
	spa.sendv_sndinfo.snd_flags |= SCTP_EOR;

	const auto& reliability = message.reliability();
	// set prinfo
	spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
	if (!reliability.ordered) {
		spa.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
	}

	if (reliability.max_rtx_count.has_value()) {
		spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
		spa.sendv_prinfo.pr_value = uint32_t(reliability.max_rtx_count.value());
	} else if (reliability.max_rtx_ms.has_value()) {
		spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
		spa.sendv_prinfo.pr_value = uint32_t(reliability.max_rtx_ms.value());
	} else {
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_NONE;
	}

	int sent_ret;
	if (available_payload_size != 0) {
		sent_ret = usrsctp_sendv(socket_, message.available_payload_data(), available_payload_size, nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
	} else {
		const char zero = 0;
		sent_ret = usrsctp_sendv(socket_, &zero, 1, nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
	}

	if (sent_ret < 0) {
		if (errno == EWOULDBLOCK) {
			PLOG_WARNING << "SCTP sending blocked.";
			ready_to_send_ = false;
			return false;
		} else if (errno == EAGAIN) {
			PLOG_WARNING << "SCTP sending not possible, try it again.";
			return false;
		} else {
			PLOG_ERROR << "SCTP sending failed, errno: " << errno;
			throw std::runtime_error("Faild to send SCTP message, errno: " + std::to_string(errno));
		}
	}

	message.Advance(size_t(sent_ret));

	PLOG_VERBOSE << "SCTP send size: " << sent_ret
				 << ", left message size: " << message.available_payload_size();

	if (message.type() == SctpMessage::Type::STRING || 
		message.type() == SctpMessage::Type::BINARY) {
		bytes_sent_ += sent_ret;
	}

	return true;
}

void SctpTransport::ReadyToSend() {
	RTC_RUN_ON(task_queue_);
	if (!ready_to_send_) {
		ready_to_send_ = true;
		if (ready_to_send_callback_) {
			ready_to_send_callback_();
		}
	}
}

void SctpTransport::DoRecv() {
	RTC_RUN_ON(task_queue_);
	try {
		while (state_ != State::DISCONNECTED && state_ != State::FAILED) {
			socklen_t from_len = 0;
			struct sctp_rcvinfo info = {};
			socklen_t info_len = sizeof(info);
			unsigned int info_type = 0;
			int flags = 0;
			ssize_t len = usrsctp_recvv(socket_, buffer_, buffer_size_, nullptr, &from_len, &info, &info_len, &info_type, &flags);

			if (len < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ECONNRESET) {
					break;
				} else {
					throw std::runtime_error("SCTP recv failed. errno: " + std::to_string(errno));
				}
			}

			PLOG_VERBOSE << "SCTP received length: " << len;

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
			} else {
				message_data_fragments_.insert(message_data_fragments_.end(), buffer_, buffer_ + len);
				if (flags & MSG_EOR) {
					if (info_type != SCTP_RECVV_RCVINFO) {
						throw std::runtime_error("Missing SCTP recv info.");
					}
					ProcessMessage(message_data_fragments_, info.rcv_sid, PayloadId(ntohl(info.rcv_ppid)));
					message_data_fragments_.clear();
				}
			}
		}
	} catch (const std::exception& exp) {
		PLOG_WARNING << exp.what();
	}
}

void SctpTransport::DoFlush() {
	RTC_RUN_ON(task_queue_);
	try {
		if (FlushPendingMessage()) {
			ReadyToSend();
		}
	} catch(const std::exception& exp) {
		PLOG_WARNING << exp.what();
	}
}

void SctpTransport::ResetStream(uint16_t stream_id) {
	RTC_RUN_ON(task_queue_);
	if (!socket_ || state_ != State::CONNECTED) {
		return;
	}

	PLOG_DEBUG << "SCTP resetting stream: " << stream_id;

	using srs_t = struct sctp_reset_streams;
	const size_t len = sizeof(srs_t) + sizeof(uint16_t);
	uint8_t buffer[len] = {};
	srs_t& srs = *reinterpret_cast<srs_t *>(buffer);
	srs.srs_flags = SCTP_STREAM_RESET_OUTGOING;
	srs.srs_number_streams = 1;
	srs.srs_stream_list[0] = stream_id;

	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_RESET_STREAMS, &srs, len) != 0) {
		if (errno == EINVAL) {
			PLOG_DEBUG << "SCTP stream: " << stream_id << " already reset.";
		} else {
			PLOG_WARNING << "SCTP reset stream " << stream_id << " failed, errno: " << std::to_string(errno);
		}
	}
}

void SctpTransport::ProcessNotification(const union sctp_notification* notification, size_t len) {
	RTC_RUN_ON(task_queue_);
	if (len != size_t(notification->sn_header.sn_length)) {
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
		} else {
			if (State() == State::CONNECTING) {
				PLOG_ERROR << "SCTP connection failed.";
				UpdateState(State::FAILED);
			} else {
				PLOG_INFO << "SCTP disconnected.";
				UpdateState(State::DISCONNECTED);
			}
		}
		break;
	}
	case SCTP_SENDER_DRY_EVENT: {
		PLOG_VERBOSE << "SCTP dry event.";
		// It should not be necessary since the sand callback should have been called already,
		// but to be sure, let's try to send now.
		DoFlush();
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
				uint16_t stream_id = reset_event.strreset_stream_list[i];
				desc << (i != 0 ? "," : "") << stream_id;
			}
			desc << "]";

			PLOG_VERBOSE << "SCTP reset event, " << desc.str();
		}

		if (flags & SCTP_STREAM_RESET_OUTGOING_SSN) {
			for (int i = 0; i < count; ++i) {
				uint16_t stream_id = reset_event.strreset_stream_list[i];
				ResetStream(stream_id);
			}
		}

		if (flags & SCTP_STREAM_RESET_INCOMING_SSN) {
			static const uint8_t data_channel_close_message{0x04};
			for (int i = 0; i < count; ++i) {
				uint16_t stream_id = reset_event.strreset_stream_list[i];
				CopyOnWriteBuffer payload(&data_channel_close_message, 1);
				SctpMessage sctp_message(SctpMessage::Type::CONTROL, stream_id, std::move(payload));
				ForwardReceivedSctpMessage(std::move(sctp_message));
			}
		}

		break;
	}
	default:
		break;
	}
}

void SctpTransport::ProcessMessage(const BinaryBuffer& message_data, uint16_t stream_id, SctpTransport::PayloadId payload_id) {
	RTC_RUN_ON(task_queue_);
	PLOG_VERBOSE << "Process message, stream id: " << stream_id << ", payload id: " << int(payload_id);

	// RFC 8831: The usage of the PPIDs "WebRTC String Partial" and "WebRTC Binary Partial" is
	// deprecated. They were used for a PPID-based fragmentation and reassembly of user messages
	// belonging to reliable and ordered data channels.
	// See https://tools.ietf.org/html/rfc8831#section-6.6
	// We handle those PPIDs at reception for compatibility reasons but shall never send them.
	switch (payload_id) {
	case PayloadId::PPID_CONTROL: {
		SctpMessage sctp_message(SctpMessage::Type::CONTROL, stream_id, message_data);
		ForwardReceivedSctpMessage(std::move(sctp_message));
		break;
	}
	case PayloadId::PPID_STRING_PARTIAL: 
		string_data_fragments_.insert(string_data_fragments_.end(), message_data.begin(), message_data.end());
		break;
	case PayloadId::PPID_STRING: {
		// Received a complete string message
		if (string_data_fragments_.empty()) {
			bytes_recv_ += message_data.size();
			SctpMessage sctp_message(SctpMessage::Type::STRING, stream_id, message_data);
			ForwardReceivedSctpMessage(std::move(sctp_message));
		} else {
			bytes_recv_ += message_data.size();
			string_data_fragments_.insert(string_data_fragments_.end(), message_data.begin(), message_data.end());
			SctpMessage sctp_message(SctpMessage::Type::STRING, stream_id, string_data_fragments_);
			ForwardReceivedSctpMessage(std::move(sctp_message));
			string_data_fragments_.clear();
		}
		break;
	}
	case PayloadId::PPID_STRING_EMPTY: {
		SctpMessage sctp_message(SctpMessage::Type::STRING, stream_id, string_data_fragments_);
		ForwardReceivedSctpMessage(std::move(sctp_message));
		string_data_fragments_.clear();
		break;
	}
	case PayloadId::PPID_BINARY_PARTIAL: 
		binary_data_fragments_.insert(binary_data_fragments_.end(), message_data.begin(), message_data.end());
		break;
	case PayloadId::PPID_BINARY: {
		if (binary_data_fragments_.empty()) {
			bytes_recv_ += message_data.size();
			SctpMessage sctp_message(SctpMessage::Type::BINARY, stream_id, message_data);
			ForwardReceivedSctpMessage(std::move(sctp_message));
		} else {
			bytes_recv_ += message_data.size();
			binary_data_fragments_.insert(binary_data_fragments_.end(), message_data.begin(), message_data.end());
			SctpMessage sctp_message(SctpMessage::Type::BINARY, stream_id, binary_data_fragments_);
			ForwardReceivedSctpMessage(std::move(sctp_message));
			binary_data_fragments_.clear();
		}
		break;
	}
	case PayloadId::PPID_BINARY_EMPTY: {
		SctpMessage sctp_message(SctpMessage::Type::BINARY, stream_id, binary_data_fragments_);
		ForwardReceivedSctpMessage(std::move(sctp_message));
		binary_data_fragments_.clear();
		break;
	}
	default:
		PLOG_VERBOSE << "Unknown PPID: " << uint32_t(payload_id);
		break;
	}
}

void SctpTransport::ProcessPendingIncomingPackets() {
	RTC_RUN_ON(task_queue_);
	while (!pending_incoming_packets_.empty()) {
		auto packet = pending_incoming_packets_.front();
		ProcessIncomingPacket(std::move(packet));
		pending_incoming_packets_.pop();
	}
}

void SctpTransport::ProcessIncomingPacket(CopyOnWriteBuffer in_packet) {
	RTC_RUN_ON(task_queue_);
	// PLOG_VERBOSE << "Process incoming SCTP packet size: " << in_packet.size();
	if (in_packet.empty()) {
		// FIXME: Empty packet means diconnection?
		UpdateState(State::DISCONNECTED);
		return;
	}

	// This will trigger 'on_sctp_upcall'
	usrsctp_conninput(this, in_packet.cdata(), in_packet.size(), 0);
}

void SctpTransport::ForwardReceivedSctpMessage(SctpMessage message) {
	RTC_RUN_ON(task_queue_);
	if (sctp_message_received_callback_) {
		sctp_message_received_callback_(std::move(message));
	}
}

void SctpTransport::Incoming(CopyOnWriteBuffer in_packet) {
	RTC_RUN_ON(task_queue_);
	// PLOG_VERBOSE << "Incoming packet size: " << in_packet.size();
	// There could be a race condition here where we receive the remote INIT before the local one is
	// sent, which would result in the connection being aborted. Therefore, we need to wait for data
	// to be sent on our side (i.e. the local INIT) before proceeding.
	if (!has_sent_once_) {
		PLOG_VERBOSE << "Pending incoming SCTP packet size: " << in_packet.size();
		pending_incoming_packets_.push(std::move(in_packet));
		return;
	}
	ProcessIncomingPacket(std::move(in_packet));
}

int SctpTransport::Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) {
	RTC_RUN_ON(task_queue_);
	return ForwardOutgoingPacket(std::move(out_packet), std::move(options));
}

// SCTP callback methods
void SctpTransport::HandleSctpUpCall() {
	RTC_RUN_ON(task_queue_);
	if (socket_ == nullptr)
		return;

	int events = usrsctp_get_events(socket_);

	if (events & SCTP_EVENT_READ) {
		PLOG_VERBOSE << "Handle SCTP upcall: do Recv";
		DoRecv();
	}

	if (events & SCTP_EVENT_WRITE) {
		// PLOG_VERBOSE << "Handle SCTP upcall: do flush";
		DoFlush();
	}
}
    
bool SctpTransport::HandleSctpWrite(CopyOnWriteBuffer data) {
	RTC_RUN_ON(task_queue_);
	// PLOG_VERBOSE << "Handle SCTP write: " << packet.size();
	int sent_size = Outgoing(std::move(data), packet_options_);
	// Reset the sent flag and ready to handle the incoming message
	if (sent_size >= 0 && !has_sent_once_) {
		PLOG_VERBOSE << "SCTP has set once";
		ProcessPendingIncomingPackets();
		has_sent_once_ = true;
	}
	return sent_size >= 0 ? true : false;
}

} // namespace naivertc