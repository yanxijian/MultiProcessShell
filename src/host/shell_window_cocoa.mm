#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>

#include <QtGui/qwindowdefs.h>

namespace mps::host
{
	void applyCocoaWindowRoundClip(WId viewId, qreal radiusPoints, bool enable)
	{
		NSView* view = (__bridge NSView*)reinterpret_cast<void*>(viewId);
		if (!view)
		{
			return;
		}
		view.wantsLayer = YES;
		CALayer* layer = view.layer;
		NSWindow* win = view.window;
		if (!enable || radiusPoints <= 0.0)
		{
			layer.mask = nil;
			layer.cornerRadius = 0.0;
			layer.masksToBounds = NO;
			if (win)
			{
				win.opaque = YES;
			}
			return;
		}
		layer.mask = nil;
		layer.cornerRadius = static_cast<CGFloat>(radiusPoints);
		layer.masksToBounds = YES;
		if (win)
		{
			win.opaque = NO;
			win.backgroundColor = NSColor.clearColor;
		}
	}
} // namespace mps::host
