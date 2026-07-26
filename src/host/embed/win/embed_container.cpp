#include "embed_container.hpp"

#include "win_capture.hpp"

#include <QResizeEvent>
#include <QShowEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
	EmbedContainer::EmbedContainer(QWidget* parent)
		: QWidget(parent)
	{
		// Defer WA_NativeWindow until applyEmbed(): an idle native HWND in the
		// stack steals Home-page clicks. Keep ancestors non-native so chrome stays Qt.
		setAttribute(Qt::WA_DontCreateNativeAncestors, true);
		setMinimumSize(200, 150);
	}

	bool EmbedContainer::foreignAlive() const
	{
#ifdef Q_OS_WIN
		if (!m_foreignWid)
		{
			return false;
		}
		return IsWindow(reinterpret_cast<HWND>(m_foreignWid)) != FALSE;
#else
		return m_foreignWid != 0;
#endif
	}

	void EmbedContainer::bind(qint64 tabId, quintptr wid)
	{
#ifdef Q_OS_WIN
		if (wid && !IsWindow(reinterpret_cast<HWND>(wid)))
		{
			wid = 0;
		}
#endif
		m_bindings.bind(tabId, static_cast<uint64_t>(wid));
		if (m_activeTabId == tabId)
		{
			activate(tabId);
		}
	}

	void EmbedContainer::unbind(qint64 tabId)
	{
		if (m_activeTabId == tabId)
		{
			clearActive(true);
		}
		m_bindings.unbind(tabId);
	}

	bool EmbedContainer::has(qint64 tabId) const
	{
		return m_bindings.has(tabId);
	}

	quintptr EmbedContainer::takeBinding(qint64 tabId)
	{
		const quintptr wid = static_cast<quintptr>(m_bindings.take(tabId));
		if (wid && m_foreignWid == wid)
		{
			releaseForeignWindow();
			m_activeTabId = 0;
		}
		else if (m_activeTabId == tabId)
		{
			m_activeTabId = 0;
		}
		return wid;
	}

	quintptr EmbedContainer::transferBinding(EmbedContainer* from, EmbedContainer* to, qint64 tabId)
	{
		if (!from)
		{
			return 0;
		}
		const quintptr wid = from->takeBinding(tabId);
		if (to && wid)
		{
			to->bind(tabId, wid);
		}
		return wid;
	}

	void EmbedContainer::activate(qint64 tabId)
	{
		m_activeTabId = tabId;
		const quintptr wid = static_cast<quintptr>(m_bindings.peek(tabId));
		if (!wid)
		{
			clearForeignWindow(true);
			return;
		}
		setForeignWindow(wid);
	}

	void EmbedContainer::clearActive(bool hide)
	{
		clearForeignWindow(hide);
		m_activeTabId = 0;
	}

	void EmbedContainer::releaseActive()
	{
		releaseForeignWindow();
		m_activeTabId = 0;
	}

	void EmbedContainer::releaseActiveIfTab(qint64 tabId)
	{
		if (m_activeTabId == tabId || (m_foreignWid && m_bindings.peek(tabId) == static_cast<uint64_t>(m_foreignWid)))
		{
			releaseActive();
		}
	}

	void EmbedContainer::reset()
	{
		releaseActive();
		m_bindings.clear();
	}

	void EmbedContainer::resyncActive()
	{
		if (!foreignAlive())
		{
			m_foreignWid = 0;
			return;
		}
#ifdef Q_OS_WIN
		const HWND host = reinterpret_cast<HWND>(winId());
		const HWND child = reinterpret_cast<HWND>(m_foreignWid);
		if (!host || GetParent(child) != host)
		{
			applyEmbed();
			return;
		}
#endif
		syncForeignGeometry();
	}

	QPixmap EmbedContainer::grabContent(qint64 tabId, QSize maxSize)
	{
		const quintptr wid = static_cast<quintptr>(m_bindings.peek(tabId));
		if (wid && m_foreignWid == wid)
		{
			const QPixmap live = grab();
			if (!live.isNull())
			{
				return live;
			}
		}
		if (wid)
		{
			return captureWindowPixmap(wid, maxSize);
		}
		return {};
	}

	void EmbedContainer::clearForeignWindow(bool hide)
	{
#ifdef Q_OS_WIN
		if (m_foreignWid && IsWindow(reinterpret_cast<HWND>(m_foreignWid)))
		{
			const HWND child = reinterpret_cast<HWND>(m_foreignWid);
			SetParent(child, nullptr);
			if (hide)
			{
				ShowWindow(child, SW_HIDE);
			}
		}
#else
		Q_UNUSED(hide);
#endif
		m_foreignWid = 0;
	}

	void EmbedContainer::releaseForeignWindow()
	{
		// Caller will reparent; avoid Hide to reduce flash during tear-out/merge.
		m_foreignWid = 0;
	}

	void EmbedContainer::setForeignWindow(quintptr wid)
	{
#ifdef Q_OS_WIN
		if (wid && !IsWindow(reinterpret_cast<HWND>(wid)))
		{
			wid = 0;
		}
#endif
		if (m_foreignWid == wid)
		{
			if (wid)
			{
				resyncActive();
			}
			return;
		}
		if (m_foreignWid)
		{
			clearForeignWindow(true);
		}
		m_foreignWid = wid;
		applyEmbed();
	}

#ifdef Q_OS_WIN
	static void ensureWindowShown(HWND hwnd)
	{
		if (!hwnd || !IsWindow(hwnd))
		{
			return;
		}
		ShowWindow(hwnd, SW_SHOWNA);
		EnableWindow(hwnd, TRUE);
	}
#endif

	void EmbedContainer::resizeEvent(QResizeEvent* event)
	{
		QWidget::resizeEvent(event);
		syncForeignGeometry();
	}

	void EmbedContainer::showEvent(QShowEvent* event)
	{
		QWidget::showEvent(event);
		applyEmbed();
	}

	void EmbedContainer::applyEmbed()
	{
#ifdef Q_OS_WIN
		if (!foreignAlive())
		{
			m_foreignWid = 0;
			return;
		}
		setAttribute(Qt::WA_NativeWindow, true);
		winId();
		const HWND host = reinterpret_cast<HWND>(winId());
		const HWND child = reinterpret_cast<HWND>(m_foreignWid);
		LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
		style |= WS_CHILD;
		style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER | WS_DLGFRAME);
		SetWindowLongPtrW(child, GWL_STYLE, style);

		LONG_PTR ex = GetWindowLongPtrW(child, GWL_EXSTYLE);
		ex &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE | WS_EX_TOOLWINDOW);
		SetWindowLongPtrW(child, GWL_EXSTYLE, ex);

		SetParent(child, host);
		SetWindowPos(child, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ensureWindowShown(child);
		syncForeignGeometry();
		InvalidateRect(child, nullptr, FALSE);
#else
		Q_UNUSED(m_foreignWid);
#endif
	}

	void EmbedContainer::syncForeignGeometry()
	{
#ifdef Q_OS_WIN
		if (!foreignAlive())
		{
			m_foreignWid = 0;
			return;
		}
		if (!testAttribute(Qt::WA_NativeWindow))
		{
			return;
		}
		winId();
		const HWND host = reinterpret_cast<HWND>(winId());
		const HWND child = reinterpret_cast<HWND>(m_foreignWid);
		RECT rc{};
		GetClientRect(host, &rc);
		const int w = qMax(1, static_cast<int>(rc.right - rc.left));
		const int h = qMax(1, static_cast<int>(rc.bottom - rc.top));
		SetWindowPos(child, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOACTIVATE);
#else
		Q_UNUSED(this);
#endif
	}
} // namespace mps::host
