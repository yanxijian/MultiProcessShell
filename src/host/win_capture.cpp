#include "win_capture.hpp"

#include <QImage>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
	QPixmap captureWindowPixmap(quintptr wid, QSize maxSize)
	{
		if (!wid || maxSize.isEmpty())
		{
			return {};
		}
#ifdef Q_OS_WIN
		const HWND hwnd = reinterpret_cast<HWND>(wid);
		if (!IsWindow(hwnd))
		{
			return {};
		}
		RECT rc{};
		if (!GetClientRect(hwnd, &rc))
		{
			return {};
		}
		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;
		if (w <= 1 || h <= 1)
		{
			return {};
		}

		HDC hdcWindow = GetDC(hwnd);
		if (!hdcWindow)
		{
			return {};
		}
		HDC hdcMem = CreateCompatibleDC(hdcWindow);
		if (!hdcMem)
		{
			ReleaseDC(hwnd, hdcWindow);
			return {};
		}
		HBITMAP hbmp = CreateCompatibleBitmap(hdcWindow, w, h);
		if (!hbmp)
		{
			DeleteDC(hdcMem);
			ReleaseDC(hwnd, hdcWindow);
			return {};
		}
		HGDIOBJ old = SelectObject(hdcMem, hbmp);

		// PrintWindow handles many layered/Qt cases better than plain BitBlt.
		if (!PrintWindow(hwnd, hdcMem, PW_CLIENTONLY))
		{
			BitBlt(hdcMem, 0, 0, w, h, hdcWindow, 0, 0, SRCCOPY);
		}

		QImage img(w, h, QImage::Format_RGB32);
		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h; // top-down
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		GetDIBits(hdcMem, hbmp, 0, UINT(h), img.bits(), &bmi, DIB_RGB_COLORS);

		SelectObject(hdcMem, old);
		DeleteObject(hbmp);
		DeleteDC(hdcMem);
		ReleaseDC(hwnd, hdcWindow);

		QPixmap pm = QPixmap::fromImage(img);
		if (pm.width() > maxSize.width() || pm.height() > maxSize.height())
		{
			pm = pm.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
		return pm;
#else
		Q_UNUSED(maxSize);
		return {};
#endif
	}
} // namespace mps::host
