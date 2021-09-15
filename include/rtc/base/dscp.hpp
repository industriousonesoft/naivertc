#ifndef _RTC_BASE_DSCP_H_
#define _RTC_BASE_DSCP_H_

#include "base/defines.hpp"

namespace naivertc {
// ToS field: Type of service, 1 byte in the IPv4 and IPv6 headers (used the left-most 3 bits),
// which is the predecessor is DSCP.
// See https://datatracker.ietf.org/doc/html/rfc1122#section-3.2.1.6
// The ToS field structure is presented below:
//   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+
// |    ToS    |        unused     |
// +---+---+---+---+---+---+---+---+
// 8 priorities represented in 3 bits (a bigger value mean higher priority): 
// 111 - 7, Network Control
// 110 - 6, Internetwork Control
// 101 - 5, Critic
// 100 - 4, Flash Override
// 011 - 3, Flash
// 010 - 2, Immediate
// 001 - 1, Priority
// 000 - 0, Routine
// Recommended use:
// p7 and p6: reserved for network control packet, like routing.
// P5: audio flow
// P4: video flow
// P3: audio control flow
// p2: normal data
// p1: default

// DSCP: Differentiated Services Code Point
// See https://datatracker.ietf.org/doc/html/rfc2474
// 1 byte in the IPv4 and IPv6 headers (used the left-most 3 bits)
//   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+
// |         DSCP          |unused |
// +---+---+---+---+---+---+---+---+
// The left-most 3 bits is CS (Class Selector, CS1 ~ CS7), which is the same as Tos field (P1 ~ P7).
// The left 3 bits is used in different ways:

// AF: Assured Forwarding
// See https://datatracker.ietf.org/doc/html/rfc2597
// 5 bits, the left-most 3 bits is based on CS1 ~ CS4, the left 2 bits is defined as the drop precedence
// 01 - 1, low drop precedence
// 10 - 2, medium drop precedence
// 11 - 3, high drop precedence
//    Drop Prec         CS1                 CS2               CS3                CS4
// +-------------+------------------+-----------------+------------------+----------------+
// |     Low     |  AF11(001'01'0)  |  AF21(010'01'0) |  AF31(011'01'0)  |  AF41(100'01'0) |
// |    Medium   |  AF12(001'10'0)  |  AF22(010'10'0) |  AF31(011'10'0)  |  AF42(100'10'0) |
// |     High    |  AF13(001'11'0)  |  AF23(010'11'0) |  AF31(011'11'0)  |  AF43(100'11'0) |
// +-------------+------------------+-----------------+------------------+----------------+

// EF: Expedited Forwarding, a fixed DSCP value 46(101'110) based on CS5
// See https://datatracker.ietf.org/doc/html/rfc2598
// Used for low loss rate, delay and jitter, like VOIP.

// CS6: DSCP value 48(110'000)
// CS7: DSCP value 56(111'000)

// Set recommended medium-priority DSCP value
// See https://datatracker.ietf.org/doc/html/draft-ietf-tsvwg-rtcweb-qos-18
// +------------------------+-------+------+-------------+-------------+
// |       Flow Type        |  Very | Low  |    Medium   |     High    |
// |                        |  Low  |      |             |             |
// +------------------------+-------+------+-------------+-------------+
// |         Audio          |  CS1  |  DF  |   EF (46)   |   EF (46)   |
// |                        |  (8)  | (0)  |             |             |
// |                        |       |      |             |             |
// | Interactive Video with |  CS1  |  DF  |  AF42, AF43 |  AF41, AF42 |
// |    or without Audio    |  (8)  | (0)  |   (36, 38)  |   (34, 36)  |
// |                        |       |      |             |             |
// | Non-Interactive Video  |  CS1  |  DF  |  AF32, AF33 |  AF31, AF32 |
// | with or without Audio  |  (8)  | (0)  |   (28, 30)  |   (26, 28)  |
// |                        |       |      |             |             |
// |          Data          |  CS1  |  DF  |     AF11    |     AF21    |
// |                        |  (8)  | (0)  |             |             |
// +------------------------+-------+------+-------------+-------------+
enum class DSCP : uint8_t {
    DSCP_DF = 0,
    DSCP_CS0 = 0,
    DSCP_CS1 = 8,
    DSCP_AF11 = 10,
    DSCP_AF12 = 12,
    DSCP_AF13 = 14,
    DSCP_CS2 = 16,
    DSCP_AF21 = 18,
    DSCP_AF22 = 20,
    DSCP_AF23 = 22,
    DSCP_CS3 = 24,
    DSCP_AF31 = 26,
    DSCP_AF32 = 28,
    DSCP_AF33 = 30,
    DSCP_CS4 = 32,
    DSCP_AF41 = 34,
    DSCP_AF42 = 36,
    DSCP_AF43 = 38,
    DSCP_CS5 = 40,
    DSCP_EF = 46,
    DSCP_CS6 = 48,
    DSCP_CS7 = 56,
};

} // namespace naivertc

#endif