#ifndef _PC_SDP_ENTRY_H_
#define _PC_SDP_ENTRY_H_

#include "common/defines.hpp"
#include "pc/sdp_defines.hpp"

#include <string>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT SDPEntry {
public:
    virtual ~SDPEntry() = default;

    virtual std::string type() const { return type_; }
    virtual std::string description() const { return description_; }
    virtual std::string mid() const { return mid_; };
    sdp::Direction direction() const { return direction_; }
    void set_direction(sdp::Direction direction);

    std::string GenerateSDP(std::string_view eol, std::string addr, std::string_view port) const;

protected:
    SDPEntry(const std::string& mline, std::string mid, sdp::Direction direction = sdp::Direction::UNKNOWN);
    virtual std::string GenerateSDPLines(std::string_view eol) const;   

    std::vector<std::string> attributes_;

private:
    std::string type_;
    std::string description_;
    std::string mid_;
    sdp::Direction direction_;
};

}

#endif