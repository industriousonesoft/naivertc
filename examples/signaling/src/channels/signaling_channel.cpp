#include "channels/signaling_channel.hpp"
#include "components/websocket.hpp"

#include <nlohmann/json.hpp>
#include <plog/Log.h>

namespace signaling {
namespace {

using json = nlohmann::json;

bool ParseURL(const std::string& signaling_url, URLParts& parts) {
    if (!URLParts::Parse(signaling_url, parts)) {
        throw std::exception();
    }
    std::string default_port;
    if (parts.scheme == "wss") {
        return true;
    } else if (parts.scheme == "ws") {
        return false;
    } else {
        throw std::exception();
    }
}

} // namespace

// ChannelImpl declaration
class ChannelImpl : public Channel {
public:
    ChannelImpl(boost::asio::io_context& ioc);
    virtual ~ChannelImpl();
    
    void Connect(std::string signaling_url, bool insecure) override;
    void Close() override;
    void Send(std::string msg) override;
    void RegisterObserver(Observer* observer) override;
    void DeregisterObserver(Observer* observer) override;

private:
    void OnConnect(boost::system::error_code ec);
    void OnClose(boost::system::error_code ec);
    void OnRead(boost::system::error_code ec,
                std::size_t bytes_transferred,
                std::string text);

    void DoRead();

private:
    boost::asio::io_context& ioc_;
    std::unique_ptr<Websocket> ws_;
    Observer* observer_;
    
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_connecting_;
    std::atomic<bool> is_closing_;
};

// ChannelImpl implement
ChannelImpl::ChannelImpl(boost::asio::io_context& ioc)
    : ioc_(ioc),
      observer_(nullptr),
      is_connected_(false) {}

ChannelImpl::~ChannelImpl() {
    Close();
    if (ws_) {
        ws_.reset();
    }
}

void ChannelImpl::Connect(std::string signaling_url, bool insecure) {
    if (signaling_url.empty()) {
        observer_->OnClosed("Invalid signaling url");
    }
    if (is_connected_ || is_connecting_) {
        return;
    }
    is_connecting_ = true;
    URLParts parts;
    
    if (ParseURL(signaling_url, parts)) {
        ws_.reset(new Websocket(ioc_, Websocket::ssl_tag(), /*insecure=*/insecure));
    } else {
        ws_.reset(new Websocket(ioc_));
    }
    ws_->Connect(signaling_url,
                 std::bind(&ChannelImpl::OnConnect, this,
                         std::placeholders::_1));
}

void ChannelImpl::Close() {
    if (!is_connected_ || is_closing_) {
        return;
    }
    is_closing_ = true;
    ws_->Close(std::bind(&ChannelImpl::OnClose, this,
                         std::placeholders::_1));
}

void ChannelImpl::Send(std::string msg) {
    if (msg.empty()) {
        return;
    }
    ws_->WriteText(msg);
}

void ChannelImpl::RegisterObserver(Observer* observer) {
    observer_ = observer;
}

void ChannelImpl::DeregisterObserver(Observer* observer) {
    if (observer_ == observer) {
        observer_ = nullptr;
    }
}

// Private methods
void ChannelImpl::OnConnect(boost::system::error_code ec) {
    is_connecting_ = false;
    if (ec) {
        is_connected_ = false;
        observer_->OnClosed(ec.message());
        return;
    }
    is_connected_ = true;
    observer_->OnConnected();
    DoRead();
}

void ChannelImpl::OnClose(boost::system::error_code ec) {
    if (ec) {
        PLOG_WARNING << "Closed with error: " << ec.message();
    }
    is_closing_ = false;
    is_connected_ = false;
    observer_->OnClosed(ec.message());
}

void ChannelImpl::OnRead(boost::system::error_code ec,
                              std::size_t bytes_transferred,
                              std::string text) {
   
    boost::ignore_unused(bytes_transferred);

    // Do not treat this as an error as this error will occur when the read process is canceled due to writing
    if (ec == boost::asio::error::operation_aborted)
        return;

    // If WebSocket returns a closed error, immediately call Close (); to exit the OnRead function.
    if (ec == boost::beast::websocket::error::closed) {
        // When WebSocket is closed by Close () ;, the function is called in the order of OnClose ();-> ReconnectAfter ();-> OnWatchdogExpired () ;.
        // WebSocket is reconnected
        Close();
        return;
    }

    if (ec) {
        PLOG_ERROR << "error: " << ec.message();
        observer_->OnClosed(ec.message());
        return;
    }

    // Check if we need to read more.
    if (!observer_->OnRead(text)) {
        return;
    }

    // Read more data.
    DoRead();
}

void ChannelImpl::DoRead() {
    ws_->Read(std::bind(&ChannelImpl::OnRead, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
}

// CreateDefaultChannel
std::unique_ptr<Channel> CreateDefaultChannel(boost::asio::io_context& ioc) {
    return std::unique_ptr<Channel>(new ChannelImpl(ioc));
}

} // namespace signaling
