#ifndef _RTC_RTCP_FIR_H_
#define _RTC_RTCP_FIR_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/psfb.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {
class CommonHeader;

// Full intra request (FIR) (RFC 5104, Section 4.3.1)
class RTC_CPP_EXPORT Fir : public Psfb {
public:
    static constexpr uint8_t kFeedbackMessageType = 4;
    struct Request {
        Request() : ssrc(0), seq_nr(0) {}
        Request(uint32_t ssrc, uint8_t seq_nr) : ssrc(ssrc), seq_nr(seq_nr) {}
        uint32_t ssrc;
        uint8_t seq_nr;
    };
public:
    Fir();
    Fir(const Fir&);
    ~Fir() override;

    void AddRequestTo(uint32_t ssrc, uint8_t seq_num) {
        fci_itmes_.emplace_back(ssrc, seq_num);
    }
    const std::vector<Request>& requests() const { return fci_itmes_; }

    bool Parse(const CommonHeader& packet);

    size_t PacketSize() const override;

    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    // The size of the Feedback Control Infomation (FCT) for the Full Intra Request
    static constexpr size_t kFciSize = 8;

    // SSRC of media source is not used in FIR packet, Shadow base functions.
    uint32_t media_ssrc() const;
    void set_media_ssrc(uint32_t ssrc);

    std::vector<Request> fci_itmes_;
};

    
} // namespace rtcp
} // namespace naivertc


#endif