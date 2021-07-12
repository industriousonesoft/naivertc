#include "pc/transports/sctp_transport.hpp"
#include "base/internals.hpp"
#include "common/utils.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <thread>
#include <chrono>

namespace naivertc {

using namespace std::chrono_literals;
using namespace utils::numeric;

void SctpTransport::Init() {
	usrsctp_init(0, &SctpTransport::sctp_send_data_ready_cb, nullptr);
	// Enable Partial Reliability Extension (RFC 3758)
	usrsctp_sysctl_set_sctp_pr_enable(1);
	// Disable Explicit Congestion Notification
	usrsctp_sysctl_set_sctp_ecn_enable(0);
}

void SctpTransport::CustomizeSctp(const SctpCustomizedSettings& settings) {
	// The default send and receive window size of usrsctp is 256KB, which is too small for realistic RTTs,
	// therefore we increase it to 1MB by defualt for better performance.
	// See https://bugzilla.mozilla.org/show_bug.cgi?id=1051685
	usrsctp_sysctl_set_sctp_recvspace(to_uint32(settings.recvBufferSize.value_or(1024 * 1024)));
	usrsctp_sysctl_set_sctp_sendspace(to_uint32(settings.sendBufferSize.value_or(1024 * 1024)));

	// Increase maxium chunks number on queue to 10KB by default
	usrsctp_sysctl_set_sctp_max_chunks_on_queue(to_uint32(settings.maxChunksOnQueue.value_or(10 * 1024)));

	// Increase initial congestion window size to 10 MTUs (RFC 6928) by default
	usrsctp_sysctl_set_sctp_initial_cwnd(to_uint32(settings.initialCongestionWindow.value_or(10)));

	// Set max burst to 10 MTUs by default (max burst is initially 0, meaning disabled)
	usrsctp_sysctl_set_sctp_max_burst_default(to_uint32(settings.maxBurst.value_or(10)));

	// Use standard SCTP congestion control (RFC 4960) by default
	// See https://github.com/paullouisageneau/libdatachannel/issues/354
	usrsctp_sysctl_set_sctp_default_cc_module(to_uint32(settings.congestionControlModule.value_or(0)));

	// Reduce SACK delay to 20ms by default (the recommended default value from RFC 4960 is 200ms)
	usrsctp_sysctl_set_sctp_delayed_sack_time_default(to_uint32(settings.delayedSackTime.value_or(20ms).count()));

	// RTO (retransmit timeout) settings
	// RFC 2988 recommends a 1s min RTO, which is very high, but TCP on linux has a 200ms min RTO
	usrsctp_sysctl_set_sctp_rto_min_default(to_uint32(settings.minRetransmitTimeout.value_or(20ms).count()));

	// Set only 10s as max RTO instead of 60s for shorter connection timeout
	usrsctp_sysctl_set_sctp_rto_max_default(to_uint32(settings.maxRetransmitTimeout.value_or(10000ms).count()));
	usrsctp_sysctl_set_sctp_init_rto_max_default(to_uint32(settings.maxRetransmitTimeout.value_or(10000ms).count()));

	// Still set 1s as initial RTO
	usrsctp_sysctl_set_sctp_rto_initial_default(to_uint32(settings.initialRetransmitTimeout.value_or(1000ms).count()));

	// RTX settings
	// 5 retransmissions instead of 8 to shorten the backoff for shorter connection timeout
	auto max_rtx_count = to_uint32(settings.maxRetransmitAttempts.value_or(5));
	usrsctp_sysctl_set_sctp_init_rtx_max_default(max_rtx_count);
	usrsctp_sysctl_set_sctp_assoc_rtx_max_default(max_rtx_count);
	usrsctp_sysctl_set_sctp_path_rtx_max_default(max_rtx_count);
	
	// Heartbeat interval 10s
	usrsctp_sysctl_set_sctp_heartbeat_interval_default(to_uint32(settings.heartbeatInterval.value_or(10000ms).count()));
}

void SctpTransport::Cleanup() {
	// Waiting util sctp finished
	while (usrsctp_finish() != 0) {
		std::this_thread::sleep_for(100ms);
	}
}

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

int SctpTransport::sctp_send_data_ready_cb(void* ptr, void* data, size_t len, uint8_t tos, uint8_t set_df) {
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