#include "rtc/rtp_rtcp/rtcp/packets/compound_packet.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/fir.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/receiver_report.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/sender_report.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kSeqNo = 13;
    
} // namespace

using namespace naivertc::rtcp;

MY_TEST(RtcpCompoundPacketTest, AppendPacket) {
    CompoundPacket compound;
    auto fir = std::make_unique<Fir>();
    fir->AddRequestTo(kRemoteSsrc, kSeqNo);
    ReportBlock rb;
    auto rr = std::make_unique<ReceiverReport>();
    rr->set_sender_ssrc(kSenderSsrc);
    EXPECT_TRUE(rr->AddReportBlock(rb));
    compound.Append(std::move(rr));
    compound.Append(std::move(fir));

    auto packet = compound.Build();
    RtcpPacketParser parser;
    parser.Parse(packet.data(), packet.size());
    EXPECT_EQ(1, parser.receiver_report()->num_packets());
    EXPECT_EQ(kSenderSsrc, parser.receiver_report()->sender_ssrc());
    EXPECT_EQ(1u, parser.receiver_report()->report_blocks().size());
    EXPECT_EQ(1, parser.fir()->num_packets());
}

MY_TEST(RtcpCompoundPacketTest, AppendPacketWithOwnAppendedPacket) {
    CompoundPacket root;
    auto leaf = std::make_unique<CompoundPacket>();

    auto fir = std::make_unique<Fir>();
    fir->AddRequestTo(kRemoteSsrc, kSeqNo);
    auto bye = std::make_unique<Bye>();
    ReportBlock rb;

    auto rr = std::make_unique<ReceiverReport>();
    EXPECT_TRUE(rr->AddReportBlock(rb));
    leaf->Append(std::move(rr));
    leaf->Append(std::move(fir));

    auto sr = std::make_unique<SenderReport>();
    root.Append(std::move(sr));
    root.Append(std::move(bye));
    root.Append(std::move(leaf));

    auto packet = root.Build();
    RtcpPacketParser parser;
    parser.Parse(packet.data(), packet.size());
    EXPECT_EQ(1, parser.sender_report()->num_packets());
    EXPECT_EQ(1, parser.receiver_report()->num_packets());
    EXPECT_EQ(1u, parser.receiver_report()->report_blocks().size());
    EXPECT_EQ(1, parser.bye()->num_packets());
    EXPECT_EQ(1, parser.fir()->num_packets());
}

MY_TEST(RtcpCompoundPacketTest, BuildWithInputBuffer) {
    CompoundPacket compound;
    auto fir = std::make_unique<Fir>();
    fir->AddRequestTo(kRemoteSsrc, kSeqNo);
    ReportBlock rb;
    auto rr = std::make_unique<ReceiverReport>();
    rr->set_sender_ssrc(kSenderSsrc);
    EXPECT_TRUE(rr->AddReportBlock(rb));
    compound.Append(std::move(rr));
    compound.Append(std::move(fir));

    const size_t kRrLength = 8;
    const size_t kReportBlockLength = 24;
    const size_t kFirLength = 20;

    const size_t kBufferSize = kRrLength + kReportBlockLength + kFirLength;
    testing::MockFunction<void(ArrayView<const uint8_t>)> callback;
    EXPECT_CALL(callback, Call(testing::_))
        .WillOnce(testing::Invoke([&](ArrayView<const uint8_t> packet) {
            RtcpPacketParser parser;
            parser.Parse(packet.data(), packet.size());
            EXPECT_EQ(1, parser.receiver_report()->num_packets());
            EXPECT_EQ(1u, parser.receiver_report()->report_blocks().size());
            EXPECT_EQ(1, parser.fir()->num_packets());
        }));

    EXPECT_TRUE(compound.Build(kBufferSize, callback.AsStdFunction()));
}
    
MY_TEST(RtcpCompoundPacketTest, BuildWithTooSmallBuffer_FragmentedSend) {
  CompoundPacket compound;
  auto fir = std::make_unique<Fir>();
  fir->AddRequestTo(kRemoteSsrc, kSeqNo);
  ReportBlock rb;
  auto rr = std::make_unique<ReceiverReport>();
  rr->set_sender_ssrc(kSenderSsrc);
  EXPECT_TRUE(rr->AddReportBlock(rb));
  compound.Append(std::move(rr));
  compound.Append(std::move(fir));

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  const size_t kBufferSize = kRrLength + kReportBlockLength;
  testing::MockFunction<void(ArrayView<const uint8_t>)> callback;
  EXPECT_CALL(callback, Call(testing::_))
      .WillOnce(testing::Invoke([&](ArrayView<const uint8_t> packet) {
        RtcpPacketParser parser;
        parser.Parse(packet.data(), packet.size());
        EXPECT_EQ(1, parser.receiver_report()->num_packets());
        EXPECT_EQ(1U, parser.receiver_report()->report_blocks().size());
        EXPECT_EQ(0, parser.fir()->num_packets());
      }))
      .WillOnce(testing::Invoke([&](ArrayView<const uint8_t> packet) {
        RtcpPacketParser parser;
        parser.Parse(packet.data(), packet.size());
        EXPECT_EQ(0, parser.receiver_report()->num_packets());
        EXPECT_EQ(0U, parser.receiver_report()->report_blocks().size());
        EXPECT_EQ(1, parser.fir()->num_packets());
      }));

  EXPECT_TRUE(compound.Build(kBufferSize, callback.AsStdFunction()));
}

} // namespace test
} // namespace naivertc
