#ifndef _RTC_RTP_PACKETIZATION_CONFIG_H_
#define _RTC_RTP_PACKETIZATION_CONFIG_H_

#include "base/defines.hpp"

#include <string>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketizationConfig {
public:
    // 对于一个系统而言，需要定义一个epoch，所有的时间表示是基于这个基准点的，
    // 对于linux而言，采用了和unix epoch一样的时间点：1970年1月1日0点0分0秒（UTC）。
    // NTP协议使用的基准点是：1900年1月1日0点0分0秒（UTC）。
    // GPS系统使用的基准点是：1980年1月6日0点0分0秒（UTC）。
    // 每个系统都可以根据自己的逻辑定义自己epoch，例如unix epoch的基准点是因为unix操作系统是在1970年左右成型的。
    // 详见 https://www.cnblogs.com/arnoldlu/p/7078179.html
    enum class EpochType : unsigned long long {
        T1970 = 2208988800UL, // Number of seconds between 1970 and 1900
        T1900 = 0
    };

public:
    /// Construct RTP configuration used in packetization process
	/// @param ssrc SSRC of source
	/// @param cname CNAME of source
	/// @param payloadType Payload type of source
	/// @param clockRate Clock rate of source used in timestamps
	/// @param sequenceNumber Initial sequence number of RTP packets (random number is choosed if nullopt)
	/// @param timestamp Initial timastamp of RTP packets (random number is choosed if nullopt)
    RtpPacketizationConfig(uint32_t ssrc, std::string cname, uint8_t payload_type, uint32_t clock_rate, 
                           std::optional<uint16_t> sequence_num = std::nullopt, 
                           std::optional<uint32_t> timestamp = std::nullopt);

    ~RtpPacketizationConfig() = default;

    uint32_t ssrc() const { return ssrc_; }
    std::string cname() const { return cname_; }
    uint8_t payload_type() const { return payload_type_; }
    uint32_t clock_rate() const { return clock_rate_; }
    uint32_t start_timestamp() const { return start_timestamp_; }
    // Seconds with epoch of Jan 1, 1900 
    double start_time_in_second() const { return start_time_s_; }
    uint16_t sequence_num() const { return sequence_num_; }
    uint32_t timestamp() const { return timestamp_; }

    /// Creates relation between time and timestamp mapping given start time and start timestamp
	/// @param startTime_s Start time of the stream
	/// @param epochStart Type of used epoch
	/// @param startTimestamp Corresponding timestamp for given start time (current timestamp will be used if value is nullopt)
    void set_start_time(double start_time_s, EpochType epoch_type, std::optional<uint32_t> start_timestamp = std::nullopt);

private:
    uint32_t ssrc_;
    std::string cname_;
    uint8_t payload_type_;
    uint32_t clock_rate_;

    uint16_t sequence_num_;
    uint32_t timestamp_;

    uint32_t start_timestamp_ = 0;
    // Seconds with epoch of Jan 1, 1900 
    double start_time_s_ = 0;
};
    
} // namespace naivertc


#endif