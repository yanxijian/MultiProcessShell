#include "embed_container.hpp"

#include "win_capture.hpp"

#include <QResizeEvent>
#include <QShowEvent>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
#ifdef Q_OS_WIN
	static void focusClientWindow(HWND child);
#endif

	EmbedContainer::EmbedContainer(QWidget* parent)
		: QWidget(parent)
	{
		// Defer WA_NativeWindow until applyEmbed(): an idle native HWND in the
		// stack steals Home-content clicks. Keep ancestors non-native so chrome stays Qt.
		setAttribute(Qt::WA_DontCreateNativeAncestors, true);
		setMinimumSize(200, 150);
	}

	bool EmbedContainer::clientWindowAlive() const
	{
#ifdef Q_OS_WIN
		if (!m_clientWid)
		{
			return false;
		}
		return IsWindow(reinterpret_cast<HWND>(m_clientWid)) != FALSE;
#else
		return m_clientWid != 0;
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
		if (wid && m_clientWid == wid)
		{
			releaseClientWindow();
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
			clearClientWindow(true);
			return;
		}
		setClientWindow(wid);
#ifdef Q_OS_WIN
		const HWND child = reinterpret_cast<HWND>(m_clientWid);
		const DWORD hostThreadId = GetCurrentThreadId();
		const DWORD clientThreadId = GetWindowThreadProcessId(child, nullptr);
		if (m_attachedClientThreadId != 0 && m_attachedClientThreadId != clientThreadId)
		{
			AttachThreadInput(hostThreadId, m_attachedClientThreadId, FALSE);
			m_attachedClientThreadId = 0;
		}
		if (clientThreadId != 0 && clientThreadId != hostThreadId && m_attachedClientThreadId == 0
			&& AttachThreadInput(hostThreadId, clientThreadId, TRUE))
		{
			m_attachedClientThreadId = clientThreadId;
		}
		const HWND host = GetAncestor(static_cast<HWND>(ensureNativeHostWindow()), GA_ROOT);
		if (host && IsWindowVisible(host))
		{
			SetForegroundWindow(host);
		}
		focusClientWindow(child);
#endif
	}

	void EmbedContainer::clearActive(bool hide)
	{
		clearClientWindow(hide);
		m_activeTabId = 0;
	}

	void EmbedContainer::releaseActive()
	{
		releaseClientWindow();
		m_activeTabId = 0;
	}

	void EmbedContainer::releaseActiveIfTab(qint64 tabId)
	{
		if (m_activeTabId == tabId || (m_clientWid && m_bindings.peek(tabId) == static_cast<uint64_t>(m_clientWid)))
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
		if (!clientWindowAlive())
		{
			m_clientWid = 0;
			return;
		}
#ifdef Q_OS_WIN
		const HWND host = static_cast<HWND>(ensureNativeHostWindow());
		const HWND child = reinterpret_cast<HWND>(m_clientWid);
		if (!host || GetParent(child) != host)
		{
			applyEmbed();
			return;
		}
#endif
		syncClientGeometry();
	}

	QPixmap EmbedContainer::grabContent(qint64 tabId, QSize maxSize)
	{
		const quintptr wid = static_cast<quintptr>(m_bindings.peek(tabId));
		if (wid && m_clientWid == wid)
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

	void EmbedContainer::clearClientWindow(bool hide)
	{
#ifdef Q_OS_WIN
		if (m_attachedClientThreadId != 0)
		{
			AttachThreadInput(GetCurrentThreadId(), m_attachedClientThreadId, FALSE);
			m_attachedClientThreadId = 0;
		}
		if (m_clientWid && IsWindow(reinterpret_cast<HWND>(m_clientWid)))
		{
			const HWND child = reinterpret_cast<HWND>(m_clientWid);
			SetParent(child, nullptr);
			if (hide)
			{
				ShowWindow(child, SW_HIDE);
			}
		}
#else
		Q_UNUSED(hide);
#endif
		m_clientWid = 0;
	}

	void EmbedContainer::releaseClientWindow()
	{
		// Caller will reparent; avoid Hide to reduce flash during tear-out/merge.
#ifdef Q_OS_WIN
		if (m_attachedClientThreadId != 0)
		{
			AttachThreadInput(GetCurrentThreadId(), m_attachedClientThreadId, FALSE);
			m_attachedClientThreadId = 0;
		}
#endif
		m_clientWid = 0;
	}

	void EmbedContainer::setClientWindow(quintptr wid)
	{
#ifdef Q_OS_WIN
		if (wid && !IsWindow(reinterpret_cast<HWND>(wid)))
		{
			wid = 0;
		}
#endif
		if (m_clientWid == wid)
		{
			if (wid)
			{
				resyncActive();
			}
			return;
		}
		if (m_clientWid)
		{
			clearClientWindow(true);
		}
		m_clientWid = wid;
		applyEmbed();
	}

#ifdef Q_OS_WIN
	static void ensureWindowShown(HWND hwnd)
	{
		if (!hwnd || !IsWindow(hwnd))
		{
			return;
		}
		ShowWindow(hwnd, SW_SHOW);
		EnableWindow(hwnd, TRUE);
	}

	static void focusClientWindow(HWND child)
	{
		if (!child || !IsWindow(child))
		{
			return;
		}

		SetFocus(child);
	}
#endif

	void EmbedContainer::resizeEvent(QResizeEvent* event)
	{
		QWidget::resizeEvent(event);
		syncClientGeometry();
	}

	void EmbedContainer::showEvent(QShowEvent* event)
	{
		QWidget::showEvent(event);
		applyEmbed();
	}

	void EmbedContainer::applyEmbed()
	{
#ifdef Q_OS_WIN
		if (!clientWindowAlive())
		{
			m_clientWid = 0;
			return;
		}
		// Qt force-nativizes the whole parent chain and all siblings when one
		// widget gets WA_NativeWindow (QWidget::setAttribute → enforceNativeChildren
		// + QWidgetPrivate::createWinId). That would hand the title bar its own
		// native HWND, which then swallows every physical mouse message over it —
		// the shell's NC hit test (caption drag / min / max / close) lives on the
		// top-level window and would go dead after the first embed. Keep native
		//-ization scoped to this container only; see ensureNativeHostWindow().
		const HWND host = static_cast<HWND>(ensureNativeHostWindow());
		const HWND child = reinterpret_cast<HWND>(m_clientWid);
		LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
		style |= WS_CHILD | WS_TABSTOP;
		style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER | WS_DLGFRAME);
		SetWindowLongPtrW(child, GWL_STYLE, style);

		LONG_PTR ex = GetWindowLongPtrW(child, GWL_EXSTYLE);
		ex &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE | WS_EX_TOOLWINDOW);
		SetWindowLongPtrW(child, GWL_EXSTYLE, ex);

		SetParent(child, host);
		SetWindowPos(child, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ensureWindowShown(child);
		syncClientGeometry();
		InvalidateRect(child, nullptr, FALSE);
#else
		Q_UNUSED(m_clientWid);
#endif
	}

	void* EmbedContainer::ensureNativeHostWindow()
	{
#ifdef Q_OS_WIN
		// Scope guards: WA_DontCreateNativeAncestors blocks the forced native
		// parent chain (QWidgetPrivate::createWinId), AA_DontCreateNativeWidgetSiblings
		// blocks enforceNativeChildren() on our parent (which would nativize the
		// title bar and every other sibling). Both only need to be in effect
		// while the WA_NativeWindow attribute change is processed.
		setAttribute(Qt::WA_DontCreateNativeAncestors, true);
		QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);
		setAttribute(Qt::WA_NativeWindow, true);
		winId();
		QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, false);
		// QWidgetPrivate::create() seeds the QWindow with the parent-relative
		// rect ("win->setGeometry(q->geometry())", qwidget.cpp); with alien
		// ancestors the QWindow is re-parented onto the top-level, so that seed
		// position is wrong (verified: the container HWND landed at window-local
		// (0,0), covering the title bar). Later moves go through
		// setGeometry_sys(), which maps via nativeParentWidget() correctly —
		// re-apply the same mapping once here to fix the seed.
		if (QWindow* wh = windowHandle())
		{
			if (QWidget* np = nativeParentWidget())
			{
				wh->setGeometry(QRect(mapTo(np, QPoint(0, 0)), size()));
			}
		}
		return reinterpret_cast<void*>(winId());
#else
		return nullptr;
#endif
	}

	void EmbedContainer::syncClientGeometry()
	{
#ifdef Q_OS_WIN
		if (!clientWindowAlive())
		{
			m_clientWid = 0;
			return;
		}
		if (!testAttribute(Qt::WA_NativeWindow))
		{
			return;
		}
		// Idempotent when already native; keeps a single acquisition pattern.
		const HWND host = static_cast<HWND>(ensureNativeHostWindow());
		const HWND child = reinterpret_cast<HWND>(m_clientWid);
		RECT rc{};
		GetClientRect(host, &rc);
		const int w = qMax(1, static_cast<int>(rc.right - rc.left));
		const int h = qMax(1, static_cast<int>(rc.bottom - rc.top));
		SetWindowPos(child, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
#else
		Q_UNUSED(this);
#endif
	}
} // namespace mps::host
