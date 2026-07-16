#include "frame.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using mps::ipc::encodeFrame;
using mps::ipc::FrameDecoder;
using mps::ipc::FrameError;
using mps::ipc::kMaxFramePayloadBytes;

TEST(FrameCodec, RoundTripEmptyPayload) {
  const auto frame = encodeFrame("");
  ASSERT_EQ(frame.size(), 4u);
  EXPECT_EQ(frame[0], 0);
  EXPECT_EQ(frame[1], 0);
  EXPECT_EQ(frame[2], 0);
  EXPECT_EQ(frame[3], 0);

  FrameDecoder dec;
  dec.append(frame.data(), frame.size());
  std::string payload;
  ASSERT_EQ(dec.tryPop(payload), FrameError::Ok);
  EXPECT_TRUE(payload.empty());
  EXPECT_EQ(dec.tryPop(payload), FrameError::Incomplete);
}

TEST(FrameCodec, RoundTripHelloBytes) {
  const std::string body = "hello-mps";
  const auto frame = encodeFrame(body);
  ASSERT_EQ(frame.size(), 4u + body.size());
  EXPECT_EQ(frame[0], 0);
  EXPECT_EQ(frame[1], 0);
  EXPECT_EQ(frame[2], 0);
  EXPECT_EQ(frame[3], static_cast<std::uint8_t>(body.size()));

  FrameDecoder dec;
  // Feed one byte at a time to exercise incremental decode.
  for (auto b : frame) {
    dec.append(&b, 1);
  }
  std::string payload;
  ASSERT_EQ(dec.tryPop(payload), FrameError::Ok);
  EXPECT_EQ(payload, body);
}

TEST(FrameCodec, TwoFramesInOneBuffer) {
  const auto f1 = encodeFrame("one");
  const auto f2 = encodeFrame("two");
  std::vector<std::uint8_t> blob;
  blob.insert(blob.end(), f1.begin(), f1.end());
  blob.insert(blob.end(), f2.begin(), f2.end());

  FrameDecoder dec;
  dec.append(blob.data(), blob.size());
  std::string a;
  std::string b;
  ASSERT_EQ(dec.tryPop(a), FrameError::Ok);
  ASSERT_EQ(dec.tryPop(b), FrameError::Ok);
  EXPECT_EQ(a, "one");
  EXPECT_EQ(b, "two");
  EXPECT_EQ(dec.tryPop(a), FrameError::Incomplete);
}

TEST(FrameCodec, RejectOversizedLength) {
  std::uint8_t header[4] = {0xff, 0xff, 0xff, 0xff};  // way above max
  FrameDecoder dec;
  dec.append(header, 4);
  std::string payload;
  EXPECT_EQ(dec.tryPop(payload), FrameError::PayloadTooLarge);
  EXPECT_TRUE(dec.failed());
  // Further appends ignored until reset.
  const auto ok = encodeFrame("x");
  dec.append(ok.data(), ok.size());
  EXPECT_EQ(dec.tryPop(payload), FrameError::PayloadTooLarge);
  dec.reset();
  dec.append(ok.data(), ok.size());
  EXPECT_EQ(dec.tryPop(payload), FrameError::Ok);
  EXPECT_EQ(payload, "x");
}

TEST(FrameCodec, EncodeRejectsTooLarge) {
  std::string huge(static_cast<std::size_t>(kMaxFramePayloadBytes) + 1u, 'a');
  const auto frame = encodeFrame(huge);
  EXPECT_TRUE(frame.empty());
}
