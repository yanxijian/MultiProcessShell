#include "envelope_builder.hpp"
#include "envelope_codec.hpp"
#include "frame.hpp"
#include "shell/ipc/v1/ipc.pb.h"

#include <gtest/gtest.h>
#include <string>

using mps::ipc::createEnvelope;
using mps::ipc::encodeFrame;
using mps::ipc::EnvelopePtr;
using mps::ipc::FrameDecoder;
using mps::ipc::FrameError;
using mps::ipc::parseEnvelope;
using mps::ipc::serializeEnvelope;

namespace
{
	EnvelopePtr makeHelloEnvelope()
	{
		auto env = createEnvelope();
		env->set_protocol(1);
		env->set_id("corr-1");
		env->set_dir(shell::ipc::v1::DIR_EVT);
		env->set_session_id(0);
		env->set_tab_id(0);
		env->set_ts_ms(1710000000000);
		auto* hello = env->mutable_hello();
		hello->set_min_protocol(1);
		hello->set_max_protocol(1);
		hello->set_pid(4242);
		hello->set_app_name("client");
		auto* caps = hello->mutable_caps();
		caps->set_embed(shell::ipc::v1::EMBED_HWND);
		caps->set_heartbeat(true);
		caps->set_invoke(true);
		caps->set_multi_tab(true);
		return env;
	}
} // namespace

TEST(EnvelopeBuilder, MakeEnvelopeAndResponse)
{
	auto env = mps::ipc::makeEnvelope(1, "id-a", shell::ipc::v1::DIR_EVT, 99, 2, 3);
	ASSERT_TRUE(env);
	EXPECT_EQ(env->protocol(), 1u);
	EXPECT_EQ(env->id(), "id-a");
	EXPECT_EQ(env->dir(), shell::ipc::v1::DIR_EVT);
	EXPECT_EQ(env->ts_ms(), 99);
	EXPECT_EQ(env->session_id(), 2);
	EXPECT_EQ(env->tab_id(), 3);

	auto res = mps::ipc::makeResponse(1, "req-1", 100, 7);
	ASSERT_TRUE(res);
	EXPECT_EQ(res->dir(), shell::ipc::v1::DIR_RES);
	EXPECT_EQ(res->id(), "req-1");
	EXPECT_EQ(res->tab_id(), 7);
	EXPECT_EQ(res->session_id(), 0);
}

TEST(EnvelopeProto, SerializeParseRoundTrip)
{
	const auto original = makeHelloEnvelope();
	ASSERT_TRUE(original);
	std::string bytes;
	ASSERT_TRUE(serializeEnvelope(*original, &bytes));
	ASSERT_FALSE(bytes.empty());

	const auto parsed = parseEnvelope(bytes);
	ASSERT_TRUE(parsed);
	EXPECT_EQ(parsed->protocol(), 1u);
	EXPECT_EQ(parsed->id(), "corr-1");
	EXPECT_EQ(parsed->dir(), shell::ipc::v1::DIR_EVT);
	ASSERT_TRUE(parsed->has_hello());
	EXPECT_EQ(parsed->hello().pid(), 4242u);
	EXPECT_EQ(parsed->hello().app_name(), "client");
	EXPECT_EQ(parsed->hello().caps().embed(), shell::ipc::v1::EMBED_HWND);
	EXPECT_TRUE(parsed->hello().caps().invoke());
}

TEST(EnvelopeProto, FramedRoundTrip)
{
	const auto original = makeHelloEnvelope();
	ASSERT_TRUE(original);
	std::string payload;
	ASSERT_TRUE(serializeEnvelope(*original, &payload));

	const auto frame = encodeFrame(payload);
	ASSERT_FALSE(frame.empty());

	FrameDecoder dec;
	dec.append(frame.data(), frame.size());
	std::string extracted;
	ASSERT_EQ(dec.tryPop(extracted), FrameError::Ok);
	EXPECT_EQ(extracted, payload);

	const auto parsed = parseEnvelope(extracted);
	ASSERT_TRUE(parsed);
	ASSERT_TRUE(parsed->has_hello());
	EXPECT_EQ(parsed->hello().app_name(), "client");
}

TEST(EnvelopeProto, CreateSubWindowTitleSchemeA)
{
	auto env = createEnvelope();
	ASSERT_TRUE(env);
	env->set_protocol(1);
	env->set_id("req-create-1");
	env->set_dir(shell::ipc::v1::DIR_REQ);
	env->set_session_id(1);
	env->set_tab_id(7);
	env->mutable_create_sub_window()->set_title("Client1-Tab2");

	std::string bytes;
	ASSERT_TRUE(serializeEnvelope(*env, &bytes));
	const auto parsed = parseEnvelope(bytes);
	ASSERT_TRUE(parsed);
	ASSERT_TRUE(parsed->has_create_sub_window());
	EXPECT_EQ(parsed->create_sub_window().title(), "Client1-Tab2");
	EXPECT_EQ(parsed->tab_id(), 7);
}

TEST(EnvelopeProto, HeartbeatRoundTrip)
{
	auto env = createEnvelope();
	ASSERT_TRUE(env);
	env->set_protocol(1);
	env->set_id("hb-1");
	env->set_dir(shell::ipc::v1::DIR_EVT);
	env->mutable_heartbeat();

	std::string bytes;
	ASSERT_TRUE(serializeEnvelope(*env, &bytes));
	const auto frame = encodeFrame(bytes);
	FrameDecoder dec;
	dec.append(frame.data(), frame.size());
	std::string payload;
	ASSERT_EQ(dec.tryPop(payload), FrameError::Ok);
	const auto parsed = parseEnvelope(payload);
	ASSERT_TRUE(parsed);
	EXPECT_TRUE(parsed->has_heartbeat());
}

TEST(EnvelopeProto, InvokeReserveRoundTrip)
{
	auto req = createEnvelope();
	ASSERT_TRUE(req);
	req->set_protocol(1);
	req->set_id("invoke-1");
	req->set_dir(shell::ipc::v1::DIR_REQ);
	auto* inv = req->mutable_invoke();
	inv->set_method("demo.ping_ui");
	inv->set_params("{\"x\":1}");

	std::string bytes;
	ASSERT_TRUE(serializeEnvelope(*req, &bytes));
	const auto frame = encodeFrame(bytes);

	FrameDecoder dec;
	dec.append(frame.data(), frame.size());
	std::string payload;
	ASSERT_EQ(dec.tryPop(payload), FrameError::Ok);

	const auto parsed = parseEnvelope(payload);
	ASSERT_TRUE(parsed);
	ASSERT_TRUE(parsed->has_invoke());
	EXPECT_EQ(parsed->invoke().method(), "demo.ping_ui");
	EXPECT_EQ(parsed->invoke().params(), "{\"x\":1}");
}
