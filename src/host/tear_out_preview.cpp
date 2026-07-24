#include "tear_out_preview.hpp"

#include <QImage>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
	TabDragGhost::TabDragGhost(QWidget* parent)
		: QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
	{
		setAttribute(Qt::WA_TranslucentBackground, true);
		setAttribute(Qt::WA_ShowWithoutActivating, true);
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		setFocusPolicy(Qt::NoFocus);
		resize(m_contentSize + QSize(m_contentOrigin.x() * 2, 8));
	}

	void TabDragGhost::setTabPixmap(const QPixmap& pm, QSize logicalContentSize)
	{
		if (pm.isNull())
		{
			m_pm = pm;
		}
		else
		{
			// Stylesheet QFrame::grab() keeps opaque rectangular corners; mask them out
			// so translucent Tool windows don't show white tips at 200% DPI.
			QImage src = pm.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
			const qreal dpr = qMax(qreal(1), pm.devicePixelRatio());
			QImage masked(src.size(), QImage::Format_ARGB32_Premultiplied);
			masked.setDevicePixelRatio(dpr);
			masked.fill(Qt::transparent);
			{
				QPainter rp(&masked);
				rp.setRenderHint(QPainter::Antialiasing, true);
				rp.setRenderHint(QPainter::SmoothPixmapTransform, true);
				const QRectF r(0, 0, src.width() / dpr, src.height() / dpr);
				// Slightly tighter than border-radius so AA fringe white tips are cut.
				QPainterPath clip;
				clip.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3.5, 3.5);
				rp.setClipPath(clip);
				rp.drawImage(QPointF(0, 0), src);
			}
			m_pm = QPixmap::fromImage(masked);
			m_pm.setDevicePixelRatio(dpr);
		}
		if (logicalContentSize.isValid() && logicalContentSize.width() > 0)
		{
			m_contentSize = logicalContentSize;
		}
		else if (!m_pm.isNull())
		{
			const qreal dpr = qMax(qreal(1), m_pm.devicePixelRatio());
			m_contentSize = QSize(qRound(m_pm.width() / dpr), qRound(m_pm.height() / dpr));
		}
		resize(m_contentSize + QSize(m_contentOrigin.x() * 2, 8));
		update();
	}

	void TabDragGhost::setPixmap(const QPixmap& pm)
	{
		setTabPixmap(pm, {});
	}

	void TabDragGhost::paintEvent(QPaintEvent* event)
	{
		Q_UNUSED(event);
		QPainter p(this);
		// Windows translucent Tool windows can keep an opaque white backing unless
		// each paint clears to transparent first — that showed as corner “tips”.
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.fillRect(rect(), Qt::transparent);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);

		const QRectF content(m_contentOrigin.x(), m_contentOrigin.y(), m_contentSize.width(), m_contentSize.height());
		// Match TabButton stylesheet border-radius: 4px.
		constexpr qreal kRadius = 4.0;

		// Soft drop shadow below the tab (lift), not above — keeps strip alignment clean.
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 36));
		p.drawRoundedRect(content.adjusted(2, 3, 2, 4), kRadius, kRadius);
		p.setBrush(QColor(0, 0, 0, 18));
		p.drawRoundedRect(content.adjusted(1, 2, 1, 3), kRadius, kRadius);

		QPainterPath clip;
		clip.addRoundedRect(content, kRadius, kRadius);
		p.setClipPath(clip);
		p.setOpacity(0.96);
		if (!m_pm.isNull())
		{
			p.drawPixmap(content.toRect(), m_pm);
		}
		else
		{
			p.setPen(QPen(QColor(40, 40, 40), 1));
			p.setBrush(QColor(245, 245, 245, 235));
			p.drawRoundedRect(content.adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
		}
	}

	TearOutPreview::TearOutPreview(QWidget* parent)
		: QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
	{
		setAttribute(Qt::WA_TranslucentBackground, true);
		setAttribute(Qt::WA_ShowWithoutActivating, true);
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		setFocusPolicy(Qt::NoFocus);
		resize(720, 480);
	}

	void TearOutPreview::setContentPixmap(const QPixmap& pm)
	{
		m_content = pm;
		update();
	}

	QRect TearOutPreview::geometryForTabContent(const QRect& globalTabContent, QSize previewSize)
	{
		if (!globalTabContent.isValid() || !previewSize.isValid())
		{
			return {};
		}
		const int tabTopLocal = kFramePad + qMax(0, (kTitleBarHeight - globalTabContent.height()) / 2);
		return QRect(QPoint(globalTabContent.x() - kTabLeftInset, globalTabContent.y() - tabTopLocal), previewSize);
	}

	void TearOutPreview::alignToTabContent(const QRect& globalTabContent)
	{
		const QRect geo = geometryForTabContent(globalTabContent, size());
		if (!geo.isValid())
		{
			return;
		}
		if (pos() != geo.topLeft())
		{
			move(geo.topLeft());
		}
		if (!isVisible())
		{
			show();
		}
	}

	void TearOutPreview::paintEvent(QPaintEvent* event)
	{
		Q_UNUSED(event);
		QPainter p(this);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		p.fillRect(rect(), Qt::transparent);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);

		const QRectF r = QRectF(rect()).adjusted(kFramePad, kFramePad, -kFramePad, -kFramePad);
		p.setPen(QPen(QColor(40, 40, 40, 180), 1.2));
		p.setBrush(QColor(248, 248, 248, 220));
		p.drawRoundedRect(r, 6, 6);

		// Title / tab bar — height matches ShellWindow (tab ghost is centered in this band).
		const QRectF titleBar(r.left(), r.top(), r.width(), kTitleBarHeight);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0xe8, 0xe8, 0xe8, 230));
		p.drawRoundedRect(titleBar, 6, 6);
		p.fillRect(QRectF(titleBar.left(), titleBar.center().y(), titleBar.width(), titleBar.height() / 2),
				   QColor(0xe8, 0xe8, 0xe8, 230));

		// Home stub so the floating client tab sits after it (wrap alignment).
		const QRectF homeChip(titleBar.left() + 8, titleBar.top() + 6, 64, titleBar.height() - 12);
		p.setPen(QPen(QColor(90, 90, 90), 1.5));
		p.setBrush(QColor(243, 243, 243, 220));
		p.drawRoundedRect(homeChip, 4, 4);
		p.setPen(QColor(40, 40, 40));
		QFont f = font();
		f.setPointSizeF(f.pointSizeF());
		p.setFont(f);
		p.drawText(homeChip, Qt::AlignCenter, QStringLiteral("Home"));

		const QRectF body = r.adjusted(8, kTitleBarHeight + 2, -8, -8);
		if (!m_content.isNull())
		{
			p.setOpacity(0.92);
			const QPixmap scaled =
				m_content.scaled(body.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
			const QRect src((scaled.width() - body.width()) / 2, (scaled.height() - body.height()) / 2,
							int(body.width()), int(body.height()));
			p.drawPixmap(body.toRect(), scaled, src.intersected(scaled.rect()));
			p.setOpacity(1.0);
		}
		else
		{
			p.setPen(QPen(QColor(160, 160, 160, 160), 1, Qt::DashLine));
			p.setBrush(QColor(235, 235, 235, 160));
			p.drawRoundedRect(body, 4, 4);
			p.setPen(QColor(100, 100, 100, 180));
			f.setBold(false);
			p.setFont(f);
			p.drawText(body, Qt::AlignCenter, QStringLiteral("Moving…"));
		}
	}

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
		GetDIBits(hdcMem, hbmp, 0, h, img.bits(), &bmi, DIB_RGB_COLORS);

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
