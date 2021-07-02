#include "pc/transports/sctp_transport.hpp"

#include <plog/Log.h>

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

InstanceGuard<SctpTransport>* s_instance_guard = new InstanceGuard<SctpTransport>();

// SctpTransport
SctpTransport::SctpTransport(std::shared_ptr<Transport> lower, const Config& config) 
    : Transport(lower),
    config_(std::move(config)) {
    InitUsrsctp(std::move(config));
}

SctpTransport::~SctpTransport() {
	Stop();

    usrsctp_deregister_address(this);
    s_instance_guard->Remove(this);
}

void SctpTransport::Start(Transport::StartedCallback callback) {
	send_queue_.Post([this, callback](){
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
	send_queue_.Post([this, callback](){
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

}

void SctpTransport::ProcessNotification(const union sctp_notification* notification, size_t len) {

}

void SctpTransport::ProcessMessage(std::vector<std::byte>&& data, uint16_t stream_id, SctpTransport::PayloadId payload_id) {

}

void SctpTransport::OnSCTPRecvDataIsReady() {
	recv_queue_.Post([this](){
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
    
int SctpTransport::OnSCTPSendDataIsReady(const void* data, size_t lent, uint8_t tos, uint8_t set_df) {
    return 0;
}

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
		Transport::Incoming(nullptr);
		return;
	}

	PLOG_VERBOSE << "Incoming size: " << in_packet->size();

	usrsctp_conninput(this, in_packet->data(), in_packet->size(), 0);
}

void SctpTransport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
	// Set recommended medium-priority DSCP value
	// See https://tools.ietf.org/html/draft-ietf-tsvwg-rtcweb-qos-18
	out_packet->set_dscp(10); // AF11: Assured Forwarding class 1, low drop probability
	return Transport::Outgoing(std::move(out_packet), callback);
}

}