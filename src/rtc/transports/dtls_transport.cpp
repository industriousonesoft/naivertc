#include "rtc/transports/dtls_transport.hpp"
#include "common/weak_ptr_manager.hpp"

#include <plog/Log.h>

#include <sys/time.h>
#include <future>

namespace naivertc {
namespace {

// Assured Forwarding class 2, low drop precedence, the recommendation for high-priority data in RFC 8837
constexpr DSCP kHandshakePacketDscp = DSCP::DSCP_AF21;
    
} // namespace


DtlsTransport::DtlsTransport(Configuration config, bool is_client, BaseTransport* lower) 
    : BaseTransport(lower),
      config_(std::move(config)),
      is_client_(is_client),
      handshake_packet_options_(PacketKind::BINARY, kHandshakePacketDscp) {
    InitOpenSSL(config_);
    WeakPtrManager::SharedInstance()->Register(this);
}

DtlsTransport::~DtlsTransport() {
    RTC_RUN_ON(&sequence_checker_);
    DeinitOpenSSL();
    WeakPtrManager::SharedInstance()->Deregister(this);
}

void DtlsTransport::OnVerify(VerifyCallback callback) {
    RTC_RUN_ON(&sequence_checker_);
    verify_callback_ = callback;
}

bool DtlsTransport::HandleVerify(std::string fingerprint) {
    RTC_RUN_ON(&sequence_checker_);
    return verify_callback_ != nullptr ? verify_callback_(std::move(fingerprint)) : false;
}

bool DtlsTransport::IsClient() const {
    RTC_RUN_ON(&sequence_checker_);
    return is_client_;
}

bool DtlsTransport::Start() {
    RTC_RUN_ON(&sequence_checker_);
    if (is_stoped_) {
        UpdateState(State::CONNECTING);
        // Start to handshake
        // TODO: Do we should use delay post to check handshake timeout in 30s here?
        InitHandshake();
        RegisterIncoming();
        is_stoped_ = false;
    }
    return true;
}

bool DtlsTransport::Stop() {
    RTC_RUN_ON(&sequence_checker_);
    if (!is_stoped_) {
        // Cut down incomming data
        DeregisterIncoming();
        // Shutdown SSL connection
        SSL_shutdown(ssl_);
        ssl_ = NULL;
        is_stoped_ = true;
        UpdateState(State::DISCONNECTED);
    }
    return true;
}

int DtlsTransport::Send(CopyOnWriteBuffer packet, PacketOptions options) {
    RTC_RUN_ON(&sequence_checker_);
    if (packet.empty() || state_ != State::CONNECTED) {
        return -1;
    }
    user_packet_options_ = std::move(options);
    int ret = SSL_write(ssl_, packet.cdata(), int(packet.size()));
    if (openssl::check(ssl_, ret)) {
        PLOG_VERBOSE_IF(false) << "Send size=" << ret;
        return ret;
    } else {
        PLOG_VERBOSE << "Failed to send size=" << ret;
        return -1;
    }
}

// Protected && private methods
void DtlsTransport::Incoming(CopyOnWriteBuffer in_packet) {
    RTC_RUN_ON(&sequence_checker_);
    if (in_packet.empty() || !ssl_) {
        return;
    }
    try {
        // PLOG_VERBOSE << "Incoming DTLS packet size: " << in_packet.size();

        // Write into SSL in BIO, and will be retrieved by SSL_read
        BIO_write(in_bio_, in_packet.cdata(), int(in_packet.size()));

        // In non-blocking mode, We may try to do handshake multiple time. 
        if (state_ == State::CONNECTING) {
            if (TryToHandshake()) {
                // DTLS Connected
                UpdateState(State::CONNECTED);
            } else {
                if (IsHandshakeTimeout()) {
                    UpdateState(State::FAILED);
                }
                return;
            }
        // Do SSL reading after connected
        } else if (state_ != State::CONNECTED) {
            PLOG_VERBOSE << "DTLS is not connected yet.";
            return;
        }

        int read_size = SSL_read(ssl_, ssl_read_buffer_, DEFAULT_SSL_BUFFER_SIZE);

        // Read failed
        if (!openssl::check(ssl_, read_size)) {
            PLOG_ERROR << "Failed to read from ssl: " << read_size;
            return;
        }

        // PLOG_VERBOSE << "SSL read size: " << read_size;

        if (read_size > 0) {
            ForwardIncomingPacket(CopyOnWriteBuffer(ssl_read_buffer_, read_size));
        }

    }catch (const std::exception& exp) {
        PLOG_WARNING << "Error occurred when processing incoming packet: " << exp.what();
    }
}

int DtlsTransport::HandleDtlsWrite(CopyOnWriteBuffer data) {
    RTC_RUN_ON(&sequence_checker_);
    if (state_ != State::CONNECTED) {
        return Outgoing(std::move(data), handshake_packet_options_);
    } else {
        PacketOptions options = user_packet_options_ ? std::move(*user_packet_options_): PacketOptions(PacketKind::BINARY, DSCP::DSCP_DF);
        return Outgoing(std::move(data), std::move(options));
    }
}
    
int DtlsTransport::Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) {
    RTC_RUN_ON(&sequence_checker_);
    return ForwardOutgoingPacket(std::move(out_packet), std::move(options));
}
    
} // namespace naivertc

