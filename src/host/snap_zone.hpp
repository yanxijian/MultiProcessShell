#ifndef __MPS_HOST_SNAP_ZONE_H__
#define __MPS_HOST_SNAP_ZONE_H__

#include <QPoint>
#include <QRect>

namespace mps::host::snap
{
	/// Snap zone under the cursor during a tab / whole-shell drag — the
	/// Chrome-style counterpart of native Aero snap for drags that run inside
	/// the OLE DoDragDrop loop (programmed moves never trigger system snap).
	/// Mirrors the native drag-to-edge matrix; extensible (halves, quarters).
	/// Maximize is the first slice.
	enum class Zone
	{
		None,
		Maximize, ///< Top edge: fill the screen's work area.
	};

	struct Result
	{
		Zone zone = Zone::None;
		/// Target rect (screen work area) when zone != None.
		QRect targetRect;
	};

	/// Pure geometry rule: which snap zone a drag occupies.
	/// `screenAvailable` — work area of the screen under the cursor.
	/// `edgeThickness` — pointer band from the screen's top edge that arms the
	/// zone; native Aero arms at ~1px, callers pass a few px for forgiveness.
	/// Judgment: inside the horizontal span AND within the top band, else None.
	[[nodiscard]] inline Result zoneAt(QPoint cursorGlobal, const QRect& screenAvailable, int edgeThickness)
	{
		Result r;
		if (edgeThickness <= 0 || !screenAvailable.isValid())
		{
			return r;
		}
		const int bandBottom = screenAvailable.top() + edgeThickness - 1;
		if (cursorGlobal.y() >= screenAvailable.top() && cursorGlobal.y() <= bandBottom && cursorGlobal.x() >= screenAvailable.left()
			&& cursorGlobal.x() <= screenAvailable.right())
		{
			r.zone = Zone::Maximize;
			r.targetRect = screenAvailable;
		}
		return r;
	}
} // namespace mps::host::snap

#endif // __MPS_HOST_SNAP_ZONE_H__
