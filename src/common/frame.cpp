#include "frame.hpp"

#include <cstring>

namespace mps::ipc
{
	namespace
	{
		[[nodiscard]] std::uint32_t readBe32(const std::uint8_t* p)
		{
			return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8)
				   | std::uint32_t{p[3]};
		}

		void writeBe32(std::uint8_t* p, std::uint32_t v)
		{
			p[0] = static_cast<std::uint8_t>((v >> 24) & 0xffu);
			p[1] = static_cast<std::uint8_t>((v >> 16) & 0xffu);
			p[2] = static_cast<std::uint8_t>((v >> 8) & 0xffu);
			p[3] = static_cast<std::uint8_t>(v & 0xffu);
		}
	} // namespace

	std::vector<std::uint8_t> encodeFrame(std::string_view payload)
	{
		if (payload.size() > kMaxFramePayloadBytes)
		{
			return {};
		}
		const auto len = static_cast<std::uint32_t>(payload.size());
		std::vector<std::uint8_t> out(4u + payload.size());
		writeBe32(out.data(), len);
		if (!payload.empty())
		{
			std::memcpy(out.data() + 4, payload.data(), payload.size());
		}
		return out;
	}

	void FrameDecoder::reset()
	{
		buffer_.clear();
		failed_ = false;
	}

	void FrameDecoder::append(std::string_view bytes)
	{
		append(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
	}

	void FrameDecoder::append(const std::uint8_t* data, std::size_t size)
	{
		if (failed_ || data == nullptr || size == 0)
		{
			return;
		}
		buffer_.insert(buffer_.end(), data, data + size);
	}

	FrameError FrameDecoder::tryPop(std::string& out)
	{
		out.clear();
		if (failed_)
		{
			return FrameError::PayloadTooLarge;
		}
		if (buffer_.size() < 4)
		{
			return FrameError::Incomplete;
		}
		const std::uint32_t len = readBe32(buffer_.data());
		if (len > kMaxFramePayloadBytes)
		{
			failed_ = true;
			buffer_.clear();
			return FrameError::PayloadTooLarge;
		}
		const std::size_t need = 4u + static_cast<std::size_t>(len);
		if (buffer_.size() < need)
		{
			return FrameError::Incomplete;
		}
		out.assign(reinterpret_cast<const char*>(buffer_.data() + 4), static_cast<std::size_t>(len));
		buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(need));
		return FrameError::Ok;
	}
} // namespace mps::ipc
