#include "frame.hpp"
#include "shell/ipc/v1/ipc.pb.h"

#include <gtest/gtest.h>

#include <string>

using mps::ipc::encodeFrame;
using mps::ipc::FrameDecoder;
using mps::ipc::FrameError;

namespace {

shell::ipc::v1::Envelope makeHelloEnvelope() {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id("corr-1");
  env.set_dir(shell::ipc::v1::DIR_EVT);
  env.set_page_id(0);
  env.set_tab_id(0);
  env.set_ts_ms(1710000000000);
  auto* hello = env.mutable_hello();
  hello->set_min_protocol(1);
  hello->set_max_protocol(1);
  hello->set_pid(4242);
  hello->set_app_name("client");
  auto* caps = hello->mutable_caps();
  caps->set_embed(shell::ipc::v1::EMBED_HWND);
  caps->set_heartbeat(true);
  caps->set_invoke(true);
  caps->set_multi_sub_window(true);
  return env;
}

}  // namespace

TEST(EnvelopeProto, SerializeParseRoundTrip) {
  const auto original = makeHelloEnvelope();
  std::string bytes;
  ASSERT_TRUE(original.SerializeToString(&bytes));
  ASSERT_FALSE(bytes.empty());

  shell::ipc::v1::Envelope parsed;
  ASSERT_TRUE(parsed.ParseFromString(bytes));
  EXPECT_EQ(parsed.protocol(), 1u);
  EXPECT_EQ(parsed.id(), "corr-1");
  EXPECT_EQ(parsed.dir(), shell::ipc::v1::DIR_EVT);
  ASSERT_TRUE(parsed.has_hello());
  EXPECT_EQ(parsed.hello().pid(), 4242u);
  EXPECT_EQ(parsed.hello().app_name(), "client");
  EXPECT_EQ(parsed.hello().caps().embed(), shell::ipc::v1::EMBED_HWND);
  EXPECT_TRUE(parsed.hello().caps().invoke());
}

TEST(EnvelopeProto, FramedRoundTrip) {
  const auto original = makeHelloEnvelope();
  std::string payload;
  ASSERT_TRUE(original.SerializeToString(&payload));

  const auto frame = encodeFrame(payload);
  ASSERT_FALSE(frame.empty());

  FrameDecoder dec;
  dec.append(frame.data(), frame.size());
  std::string extracted;
  ASSERT_EQ(dec.tryPop(extracted), FrameError::Ok);
  EXPECT_EQ(extracted, payload);

  shell::ipc::v1::Envelope parsed;
  ASSERT_TRUE(parsed.ParseFromString(extracted));
  ASSERT_TRUE(parsed.has_hello());
  EXPECT_EQ(parsed.hello().app_name(), "client");
}

TEST(EnvelopeProto, CreateWindowTitleSchemeA) {
  shell::ipc::v1::Envelope env;
  env.set_protocol(1);
  env.set_id("req-create-1");
  env.set_dir(shell::ipc::v1::DIR_REQ);
  env.set_page_id(1);
  env.set_tab_id(7);
  env.mutable_create_window()->set_title("Client1-Window2");

  std::string bytes;
  ASSERT_TRUE(env.SerializeToString(&bytes));
  shell::ipc::v1::Envelope parsed;
  ASSERT_TRUE(parsed.ParseFromString(bytes));
  ASSERT_TRUE(parsed.has_create_window());
  EXPECT_EQ(parsed.create_window().title(), "Client1-Window2");
  EXPECT_EQ(parsed.tab_id(), 7);
}

TEST(EnvelopeProto, InvokeReserveRoundTrip) {
  shell::ipc::v1::Envelope req;
  req.set_protocol(1);
  req.set_id("invoke-1");
  req.set_dir(shell::ipc::v1::DIR_REQ);
  auto* inv = req.mutable_invoke();
  inv->set_method("demo.ping_ui");
  inv->set_params("{\"x\":1}");

  std::string bytes;
  ASSERT_TRUE(req.SerializeToString(&bytes));
  const auto frame = encodeFrame(bytes);

  FrameDecoder dec;
  dec.append(frame.data(), frame.size());
  std::string payload;
  ASSERT_EQ(dec.tryPop(payload), FrameError::Ok);

  shell::ipc::v1::Envelope parsed;
  ASSERT_TRUE(parsed.ParseFromString(payload));
  ASSERT_TRUE(parsed.has_invoke());
  EXPECT_EQ(parsed.invoke().method(), "demo.ping_ui");
  EXPECT_EQ(parsed.invoke().params(), "{\"x\":1}");
}
