#ifndef __MPS_HOST_WIN_CAPTURE_H__
#define __MPS_HOST_WIN_CAPTURE_H__

#include <QPixmap>
#include <QSize>
#include <QtGlobal>

namespace mps::host
{
	/// Platform window snapshot for drag previews / embed grab fallback (Win: PrintWindow).
	[[nodiscard]] QPixmap captureWindowPixmap(quintptr wid, QSize maxSize);
} // namespace mps::host

#endif // __MPS_HOST_WIN_CAPTURE_H__
