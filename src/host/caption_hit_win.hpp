#ifndef __MPS_HOST_CAPTION_HIT_WIN_H__
#define __MPS_HOST_CAPTION_HIT_WIN_H__

namespace mps::host
{
	/// RAII pause for the process-wide non-client hit test, held while a
	/// QDrag::exec loop runs (tab tear-out / merge). Every ShellWindow in this
	/// process reports plain client area while paused, so Qt drag events keep
	/// reaching the target shell's title bar widgets. Resets on every exit path,
	/// including a cancelled OLE drag. Main-thread only.
	class CaptionHitPauseGuard final
	{
	public:
		CaptionHitPauseGuard();
		~CaptionHitPauseGuard();
		CaptionHitPauseGuard(const CaptionHitPauseGuard&) = delete;
		CaptionHitPauseGuard& operator=(const CaptionHitPauseGuard&) = delete;
	};
} // namespace mps::host

#endif // __MPS_HOST_CAPTION_HIT_WIN_H__
