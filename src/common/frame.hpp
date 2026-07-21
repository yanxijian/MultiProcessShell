#ifndef __MPS_IPC_FRAME_H__
#define __MPS_IPC_FRAME_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mps::ipc
{
	/// Maximum Envelope payload size accepted by the decoder (16 MiB).
	inline constexpr std::uint32_t kMaxFramePayloadBytes = 16u * 1024u * 1024u;

	enum class FrameError
	{
		Ok = 0,
		Incomplete,
		PayloadTooLarge,
		InvalidArgument,
	};

	/// Encode one frame: 4-byte big-endian length + payload bytes.
	[[nodiscard]] std::vector<std::uint8_t> encodeFrame(std::string_view payload);

	/// Incremental decoder for a stream of length-prefixed frames.
	class FrameDecoder
	{
	public:
		void reset();

		/// Append raw bytes from the transport.
		void append(std::string_view bytes);
		void append(const std::uint8_t* data, std::size_t size);

		/// Try to extract the next complete payload.
		/// @return FrameError::Ok and fills @p out on success;
		///         Incomplete if more data is needed;
		///         PayloadTooLarge if declared length exceeds kMaxFramePayloadBytes
		///         (decoder enters a failed state until reset()).
		[[nodiscard]] FrameError tryPop(std::string& out);

		[[nodiscard]] bool failed() const
		{
			return failed_;
		}
		[[nodiscard]] std::size_t bufferedSize() const
		{
			return buffer_.size();
		}

	private:
		std::vector<std::uint8_t> buffer_;
		bool failed_ = false;
	};
} // namespace mps::ipc

#endif  // __MPS_IPC_FRAME_H__
