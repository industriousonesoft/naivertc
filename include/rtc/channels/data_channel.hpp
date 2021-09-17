#ifndef _RTC_CHANNELS_DATA_CHANNEL_H_
#define _RTC_CHANNELS_DATA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/transports/sctp_message.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "common/task_queue.hpp"
#include "rtc/channels/channel.hpp"

#include <memory>
#include <queue>
#include <functional>

namespace naivertc {

class SctpTransport;

class RTC_CPP_EXPORT DataChannel : public Channel, 
                                   public std::enable_shared_from_this<DataChannel> {
public:
    struct RTC_CPP_EXPORT Init {
        std::string label;
        std::string protocol = "";
        std::optional<uint16_t> stream_id = std::nullopt;
        bool unordered = false;
        bool negotiated = false;

        Init(const std::string label, 
             const std::string protocol = "", 
             std::optional<uint16_t> stream_id = std::nullopt, 
             bool unordered = false,
             bool negotiated = false);
    };

    using BinaryMessageReceivedCallback = std::function<void(const uint8_t* in_data, size_t in_size)>;
    using TextMessageReceivedCallback = std::function<void(const std::string text)>;
    using BufferedAmountChangedCallback = std::function<void(uint64_t previous_amount)>;
public:
    static std::shared_ptr<DataChannel> RemoteDataChannel(uint16_t stream_id,
                                                          bool negotiated,
                                                          std::weak_ptr<SctpTransport> sctp_transport);
public:
    DataChannel(const std::string label, 
                const std::string protocol, 
                const uint16_t stream_id, 
                bool unordered,
                bool negotiated);
    virtual ~DataChannel();

    uint16_t stream_id() const;
    const std::string label() const;
    const std::string protocol() const;
    bool is_opened() const;

    void HintStreamId(sdp::Role role);

    void Open(std::weak_ptr<SctpTransport> sctp_transport);
    void Close() override;
    void RemoteClose();

    void Send(const std::string text);
    void OnIncomingMessage(SctpMessage message);
    void OnBufferedAmount(size_t amount);
    static bool IsOpenMessage(SctpMessage message);

    void OnOpened(OpenedCallback callback) override;
    void OnClosed(ClosedCallback callback) override;
    void OnMessageReceived(BinaryMessageReceivedCallback callback);
    void OnMessageReceived(TextMessageReceivedCallback callback);
    void OnBufferedAmountChanged(BufferedAmountChangedCallback callback);

private:
    void OnOpenMessageReceived(const SctpMessage& open_message);
    void ProcessOpenMessage(const SctpMessage& open_message);
    void ProcessPendingMessages();

    void SendOpenMessage() const;
    void SendAckMessage() const;
    void SendCloseMessage() const;

    void TriggerOpen();
    void TriggerClose();
    void TriggerBufferedAmount(size_t amount);

private:
    std::string label_;
    std::string protocol_;
    uint16_t stream_id_;
    const bool negotiated_ = false;
    std::shared_ptr<SctpMessage::Reliability> reliability_;
    
    TaskQueue task_queue_;

    bool is_opened_ = false;
    std::weak_ptr<SctpTransport> sctp_transport_;

    std::queue<SctpMessage> recv_message_queue_;

    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;
    BinaryMessageReceivedCallback binary_message_received_callback_ = nullptr;
    TextMessageReceivedCallback text_message_received_callback_ = nullptr;
    BufferedAmountChangedCallback buffered_amount_changed_callback_ = nullptr;
};

} // namespace naivertc

#endif