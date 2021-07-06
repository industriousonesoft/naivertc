#include "pc/transports/sctp_transport.hpp"
#include "base/internals.hpp"
#include "common/utils.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

namespace naivertc {

void SctpTransport::InitUsrsctp(const Config& config) {
    PLOG_VERBOSE << "Initializing SCTP transport.";

    // Register this class as an address for usrsctp, This is used by SCTP to 
    // direct the packets received by sctp socket to this class.
    usrsctp_register_address(this);

    // usrsctp_socket(domain, type, protocol, recv_callback, send_callback, sd_threshold, ulp_info)
    socket_ = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, nullptr, nullptr, 0, nullptr);

    if (!socket_) {
        throw std::runtime_error("Failed to create SCTP socket, errno: " + std::to_string(errno));
    }

    usrsctp_set_upcall(socket_, &SctpTransport::sctp_recv_data_ready_cb, this);

    if (usrsctp_set_non_blocking(socket_, 1) != 0) {
        throw std::runtime_error("Unable to set non-blocking mode, errno: " + std::to_string(errno));
    }

    // SCTP must stop sending after the lower layer is shut down, so disable linger
    struct linger sol = {};
    sol.l_linger = 0;
    sol.l_onoff = 1;
    if (usrsctp_setsockopt(socket_, SOL_SOCKET, SO_LINGER, &sol, sizeof(sol))) {
        throw std::runtime_error("Could not set socket option SO_LINGER, errno: " + std::to_string(errno));
    }

    // Allow reset streams
    struct sctp_assoc_value av = {};
	av.assoc_id = SCTP_ALL_ASSOC;
	av.assoc_value = 1;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av, sizeof(av)))
		throw std::runtime_error("Could not set socket option SCTP_ENABLE_STREAM_RESET, errno=" +
		                         std::to_string(errno));
    
	int on = 1;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_RECVRCVINFO, &on, sizeof(on)))
		throw std::runtime_error("Could set socket option SCTP_RECVRCVINFO, errno=" +
		                         std::to_string(errno));

    // subscribe STCP events
    struct sctp_event se = {};
	se.se_assoc_id = SCTP_ALL_ASSOC;
	se.se_on = 1;
	se.se_type = SCTP_ASSOC_CHANGE;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_EVENT, &se, sizeof(se)))
		throw std::runtime_error("Could not subscribe to event SCTP_ASSOC_CHANGE, errno=" +
		                         std::to_string(errno));
	se.se_type = SCTP_SENDER_DRY_EVENT;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_EVENT, &se, sizeof(se)))
		throw std::runtime_error("Could not subscribe to event SCTP_SENDER_DRY_EVENT, errno=" +
		                         std::to_string(errno));
	se.se_type = SCTP_STREAM_RESET_EVENT;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_EVENT, &se, sizeof(se)))
		throw std::runtime_error("Could not subscribe to event SCTP_STREAM_RESET_EVENT, errno=" +
		                         std::to_string(errno));

	// RFC 8831 6.6. Transferring User Data on a Data Channel
	// The sender SHOULD disable the Nagle algorithm (see [RFC1122) to minimize the latency
	// See https://tools.ietf.org/html/rfc8831#section-6.6
	int nodelay = 1;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, sizeof(nodelay)))
		throw std::runtime_error("Could not set socket option SCTP_NODELAY, errno=" +
		                         std::to_string(errno));

	struct sctp_paddrparams spp = {};
	// Enable SCTP heartbeats
	spp.spp_flags = SPP_HB_ENABLE;

	// RFC 8261 5. DTLS considerations:
	// If path MTU discovery is performed by the SCTP layer and IPv4 is used as the network-layer
	// protocol, the DTLS implementation SHOULD allow the DTLS user to enforce that the
	// corresponding IPv4 packet is sent with the Don't Fragment (DF) bit set. If controlling the DF
	// bit is not possible (for example, due to implementation restrictions), a safe value for the
	// path MTU has to be used by the SCTP stack. It is RECOMMENDED that the safe value not exceed
	// 1200 bytes.
	// See https://tools.ietf.org/html/rfc8261#section-5
#if ENABLE_PMTUD
	if (!config.mtu.has_value()) {
#else
	if (false) {
#endif
		// Enable SCTP path MTU discovery
		spp.spp_flags |= SPP_PMTUD_ENABLE;
		PLOG_VERBOSE << "Path MTU discovery enabled";

	} else {
		// Fall back to a safe MTU value.
		spp.spp_flags |= SPP_PMTUD_DISABLE;
		// The MTU value provided specifies the space available for chunks in the
		// packet, so we also subtract the SCTP header size.
		size_t pmtu = config.mtu.value_or(DEFAULT_MTU_SIZE) - 12 - 37 - 8 - 40; // SCTP/DTLS/UDP/IPv6
		spp.spp_pathmtu = utils::numeric::to_uint32(pmtu);
		PLOG_VERBOSE << "Path MTU discovery disabled, SCTP MTU set to " << pmtu;
	}

	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &spp, sizeof(spp)))
		throw std::runtime_error("Could not set socket option SCTP_PEER_ADDR_PARAMS, errno=" +
		                         std::to_string(errno));

	// RFC 8831 6.2. SCTP Association Management
	// The number of streams negotiated during SCTP association setup SHOULD be 65535, which is the
	// maximum number of streams that can be negotiated during the association setup.
	// See https://tools.ietf.org/html/rfc8831#section-6.2
	struct sctp_initmsg sinit = {};
	sinit.sinit_num_ostreams = 65535;
	sinit.sinit_max_instreams = 65535;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_INITMSG, &sinit, sizeof(sinit)))
		throw std::runtime_error("Could not set socket option SCTP_INITMSG, errno=" +
		                         std::to_string(errno));

	// Prevent fragmented interleave of messages (i.e. level 0), see RFC 6458 section 8.1.20.
	// Unless the user has set the fragmentation interleave level to 0, notifications
	// may also be interleaved with partially delivered messages.
	int level = 0;
	if (usrsctp_setsockopt(socket_, IPPROTO_SCTP, SCTP_FRAGMENT_INTERLEAVE, &level, sizeof(level)))
		throw std::runtime_error("Could not disable SCTP fragmented interleave, errno=" +
		                         std::to_string(errno));

	int rcvBuf = 0;
	socklen_t rcvBufLen = sizeof(rcvBuf);
	if (usrsctp_getsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &rcvBuf, &rcvBufLen))
		throw std::runtime_error("Could not get SCTP recv buffer size, errno=" +
		                         std::to_string(errno));
	int sndBuf = 0;
	socklen_t sndBufLen = sizeof(sndBuf);
	if (usrsctp_getsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &sndBuf, &sndBufLen))
		throw std::runtime_error("Could not get SCTP send buffer size, errno=" +
		                         std::to_string(errno));

	// Ensure the buffer is also large enough to accomodate the largest messages
	const size_t maxMessageSize = config.max_message_size.value_or(DEFAULT_LOCAL_MAX_MESSAGE_SIZE);
	const int minBuf = int(std::min(maxMessageSize, size_t(std::numeric_limits<int>::max())));
	rcvBuf = std::max(rcvBuf, minBuf);
	sndBuf = std::max(sndBuf, minBuf);

	if (usrsctp_setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof(rcvBuf)))
		throw std::runtime_error("Could not set SCTP recv buffer size, errno=" +
		                         std::to_string(errno));

	if (usrsctp_setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &sndBuf, sizeof(sndBuf)))
		throw std::runtime_error("Could not set SCTP send buffer size, errno=" +
		                         std::to_string(errno));
}

// usrsctp callbacks
void SctpTransport::sctp_recv_data_ready_cb(struct socket* socket, void* arg, int flags) {
    auto* transport = static_cast<SctpTransport*>(arg);
    if (WeakPtrManager::SharedInstance()->TryLock(transport)) {
        transport->OnSCTPRecvDataIsReady();
    }
}

int SctpTransport::sctp_send_data_ready_cb(void* ptr, const void* data, size_t len, uint8_t tos, uint8_t set_df) {
    auto* transport = static_cast<SctpTransport*>(ptr);
    // In case of Sending callback is invoked on a already closed registered class instance.(transport).
    // https://github.com/sctplab/usrsctp/issues/405
    if (WeakPtrManager::SharedInstance()->TryLock(transport)) {
        return transport->OnSCTPSendDataIsReady(data, len, tos, set_df);
    }else {
        return -1;
    }
}

} // namespace naivertc