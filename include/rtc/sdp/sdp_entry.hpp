#ifndef _RTC_SDP_ENTRY_H_
#define _RTC_SDP_ENTRY_H_

#include "base/defines.hpp"
#include "rtc/sdp/sdp_defines.hpp"

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace naivertc {
namespace sdp {

// Entry
struct RTC_CPP_EXPORT Entry : public std::enable_shared_from_this<Entry> {
public:
    enum class Type {
        AUDIO,
        VIDEO,
        APPLICATION
    };
public:
    virtual ~Entry() = default;

    virtual std::string description() const { return description_; }
    virtual void ParseSDPLine(std::string_view line);

    Type type() const { return type_; }
    std::string type_string() const { return type_string_; }
    std::string mid() const { return mid_; };
    Direction direction() const { return direction_; }
    void set_direction(Direction direction);

    std::string GenerateSDP(std::string_view eol, std::string_view addr, std::string_view port) const;

protected:
    Entry(const std::string& mline, std::string mid, Direction direction = Direction::UNKNOWN);
    virtual std::string GenerateSDPLines(std::string_view eol) const;   

    std::vector<std::string> attributes_;

    Type type_string_to_type(std::string type_string) const;

private:
    Type type_;
    std::string type_string_;
    std::string description_;
    std::string mid_;
    Direction direction_;
};

} // namespace sdp
} // namespace naivert 

#endif