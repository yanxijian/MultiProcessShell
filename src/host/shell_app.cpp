#include "shell_app.hpp"

#include "tab_strip.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEasingCurve>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLocalSocket>
#include <QMimeData>
#include <QPropertyAnimation>
#include <QSize>
#include <QTimer>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
	ShellApp::ShellApp(QString clientExe, QObject* parent)
		: QObject(parent)
		, m_clientExe(std::move(clientExe))
	{
		m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
		m_endpoint = QStringLiteral("mps-demo-%1").arg(m_token);
		m_server = new QLocalServer(this);
		QLocalServer::removeServer(m_endpoint);
		if (!m_server->listen(m_endpoint))
		{
			qWarning("Failed to listen on %s", qPrintable(m_endpoint));
		}
		connect(m_server, &QLocalServer::newConnection, this, &ShellApp::onNewConnection);

		// Tool windows: no QWidget parent (ShellApp is QObject-only). Owned explicitly.
		m_tearOutPreview = new TearOutPreview(nullptr);
		m_tearOutPreview->hide();
		m_tabDragGhost = new TabDragGhost(nullptr);
		m_tabDragGhost->hide();
		m_dragVisualTimer = new QTimer(this);
		m_dragVisualTimer->setInterval(16);
		connect(m_dragVisualTimer, &QTimer::timeout, this, &ShellApp::updateTabDragVisuals);
	}

	ShellApp::~ShellApp()
	{
		delete m_tearOutPreview;
		m_tearOutPreview = nullptr;
		delete m_tabDragGhost;
		m_tabDragGhost = nullptr;
	}

	ShellWindow* ShellApp::createShell(QPoint pos, QSize size, bool showNow)
	{
		auto shell = std::make_unique<ShellWindow>(this);
		auto* raw = shell.get();
		bindShell(raw);
		if (size.isValid() && size.width() > 200 && size.height() > 150)
		{
			raw->resize(size);
		}
		if (!pos.isNull())
		{
			raw->move(pos);
		}
		if (showNow)
		{
			raw->show();
		}
		m_shells.push_back(std::move(shell));
		return raw;
	}

	void ShellApp::bindShell(ShellWindow* shell)
	{
		connect(shell, &ShellWindow::createClientClicked, this,
				[this, shell]
				{
					createClientOn(shell);
				});
		connect(shell, &ShellWindow::tabCloseRequested, this,
				[this](qint64 tabId)
				{
					closeTab(tabId);
				});
		connect(shell, &ShellWindow::tabActivated, this,
				[this, shell](qint64 tabId)
				{
					activateTab(shell, tabId);
				});
		connect(shell, &ShellWindow::tabTearOutRequested, this,
				[this, shell](qint64 tabId, QRect geom)
				{
					tearOutTab(shell, tabId, geom);
				});
		connect(shell, &ShellWindow::shellCloseRequested, this, &ShellApp::closeShell);
		connect(shell, &ShellWindow::dropIndicatorsClearRequested, this, &ShellApp::clearAllDropIndicators);

		shell->installStripDropFilter(this);
	}

	void ShellApp::clearAllDropIndicators()
	{
		for (auto& shell : m_shells)
		{
			if (shell)
			{
				shell->clearDropInsertIndicator();
			}
		}
	}

	void ShellApp::clearAllTabYieldPreviews()
	{
		for (auto& shell : m_shells)
		{
			if (shell)
			{
				shell->clearTabYieldPreview();
			}
		}
	}

	ShellWindow* ShellApp::shellFromStripDropTarget(QObject* watched) const
	{
		if (auto* shell = qobject_cast<ShellWindow*>(watched))
		{
			return shell;
		}
		for (const auto& shell : m_shells)
		{
			if (shell && shell->isStripDropTarget(watched))
			{
				return shell.get();
			}
		}
		return nullptr;
	}

	bool ShellApp::eventFilter(QObject* watched, QEvent* event)
	{
		// Esc during tab drag → cancel (browser-style), do not tear out.
		// Note: on Windows, OLE DnD often swallows KeyPress; see pollEscapeCancel_().
		if (m_dragActive && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride))
		{
			auto* ke = static_cast<QKeyEvent*>(event);
			if (ke->key() == Qt::Key_Escape)
			{
				if (!m_dragCancelled)
				{
					m_dragCancelled = true;
					startGhostSnapBack();
				}
				return false; // let QDrag / OLE also abort
			}
		}

		// Caption strip still uses ShellWindow's own filter for system-move.
		if (event->type() == QEvent::MouseButtonPress)
		{
			return QObject::eventFilter(watched, event);
		}

		auto* shell = shellFromStripDropTarget(watched);
		if (!shell)
		{
			return QObject::eventFilter(watched, event);
		}

		if (event->type() == QEvent::DragLeave)
		{
			// Do not clear yield here: Qt often sends DragLeave immediately before Drop,
			// and clearing would lose the insert index / reopen the gap under the ghost.
			shell->clearDropInsertIndicator();
			return false;
		}

		if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)
		{
			auto* de = static_cast<QDragMoveEvent*>(event);
			if (!de->mimeData()->hasFormat(QString::fromUtf8(kTabMimeType)))
			{
				return false;
			}
			QPoint dropGlobal = QCursor::pos();
			if (auto* w = qobject_cast<QWidget*>(watched))
			{
				dropGlobal = w->mapToGlobal(de->position().toPoint());
			}
			const qint64 tabId = de->mimeData()->data(QString::fromUtf8(kTabMimeType)).toLongLong();
			const int guestW =
				m_dragTabWidth > 0 ? m_dragTabWidth : (m_tabDragGhost ? m_tabDragGhost->contentSize().width() : 80);

			// Only one shell shows strip feedback at a time.
			for (auto& s : m_shells)
			{
				if (!s || s.get() == shell)
				{
					continue;
				}
				s->clearDropInsertIndicator();
				s->clearTabYieldPreview();
			}

			if (m_dragSource && shell == m_dragSource && tabId == m_dragTabId)
			{
				shell->previewTabYieldAtCursor(tabId, QCursor::pos(), 0,
											   m_tabGhostHotSpot.x()
												   - (m_tabDragGhost ? m_tabDragGhost->contentOrigin().x() : 0));
			}
			else
			{
				if (m_dragSource && m_dragSource != shell)
				{
					m_dragSource->clearTabYieldPreview();
				}
				// Merge target: live tab yield, not only a blue bar.
				shell->clearDropInsertIndicator();
				shell->previewTabYieldAtCursor(tabId, dropGlobal, guestW,
											   m_tabGhostHotSpot.x()
												   - (m_tabDragGhost ? m_tabDragGhost->contentOrigin().x() : 0));
			}
			de->acceptProposedAction();
			return true;
		}
		if (event->type() == QEvent::Drop)
		{
			auto* de = static_cast<QDropEvent*>(event);
			if (!de->mimeData()->hasFormat(QString::fromUtf8(kTabMimeType)))
			{
				return false;
			}
			const qint64 tabId = de->mimeData()->data(QString::fromUtf8(kTabMimeType)).toLongLong();
			QPoint dropGlobal = QCursor::pos();
			if (auto* w = qobject_cast<QWidget*>(watched))
			{
				dropGlobal = w->mapToGlobal(de->position().toPoint());
			}
			int insertIndex = shell->yieldInsertIndex();
			if (insertIndex < 0)
			{
				insertIndex = shell->tabInsertIndexAt(dropGlobal);
			}
			// Capture before clears — yield layout must not be required after this.
			const int mergeIndex = insertIndex;
			auto* source = shellForTab(tabId);
			if (!source)
			{
				clearAllDropIndicators();
				clearAllTabYieldPreviews();
				de->acceptProposedAction();
				return true;
			}
			if (source == shell)
			{
				if (!(shell->hasTabYieldPreview() && shell->commitTabYieldPreview()))
				{
					clearAllDropIndicators();
					clearAllTabYieldPreviews();
					shell->moveTab(tabId, mergeIndex);
				}
				else
				{
					clearAllDropIndicators();
				}
				noteTabDragDropHandled();
				de->acceptProposedAction();
				return true;
			}
			clearAllDropIndicators();
			clearAllTabYieldPreviews();
			mergeTab(tabId, shell, mergeIndex);
			noteTabDragDropHandled();
			de->acceptProposedAction();
			return true;
		}
		return QObject::eventFilter(watched, event);
	}

	void ShellApp::createClientOn(ShellWindow* shell)
	{
		const int clientIndex = m_nextClientIndex++;
		m_nextWindowIndex[clientIndex] = 1;
		auto session = std::make_unique<ClientSession>(clientIndex, m_endpoint, this);
		auto* raw = session.get();
		m_pendingFirstShell.insert(raw, shell);
		connect(raw, &ClientSession::sessionHelloOk, this, &ShellApp::onSessionReady);
		connect(raw, &ClientSession::subWindowAdded, this, &ShellApp::onSubWindowAdded);
		connect(raw, &ClientSession::subWindowRemoved, this, &ShellApp::onSubWindowRemoved);
		connect(raw, &ClientSession::sessionDead, this, &ShellApp::onSessionDead);
		connect(raw, &ClientSession::invokeNewWindow, this,
				[this](ClientSession* session, qint64 sourceTabId)
				{
					const int m = m_nextWindowIndex[session->clientIndex()]++;
					const qint64 tabId = m_nextTabId++;
					const QString title = makeTitle(session->clientIndex(), m);
					// Prefer the shell that hosts the page where the user clicked.
					ShellWindow* shell = shellForTab(sourceTabId);
					if (!shell)
					{
						// Fallback: any shell that already hosts this session (prefer last match).
						for (auto& s : m_shells)
						{
							for (const auto& t : s->tabs())
							{
								if (t.session == session)
								{
									shell = s.get();
								}
							}
						}
					}
					if (!shell && !m_shells.empty())
					{
						shell = m_shells.back().get();
					}
					if (shell)
					{
						m_pendingFirstShell.insert(session, shell);
					}
					session->requestCreateSubWindow(tabId, title);
				});
		raw->startClientProcess(m_clientExe, m_token);
		m_sessions.push_back(std::move(session));
	}

	void ShellApp::onNewConnection()
	{
		while (m_server->hasPendingConnections())
		{
			auto* sock = m_server->nextPendingConnection();
			// Attach to the newest session still without a socket.
			ClientSession* target = nullptr;
			for (auto it = m_sessions.rbegin(); it != m_sessions.rend(); ++it)
			{
				if (!(*it)->channel())
				{
					target = it->get();
					break;
				}
			}
			if (!target)
			{
				sock->abort();
				sock->deleteLater();
				continue;
			}
			target->attachSocket(sock);
		}
	}

	void ShellApp::onSessionReady(ClientSession* session)
	{
		auto* shell = m_pendingFirstShell.value(session, nullptr);
		if (!shell)
		{
			return;
		}
		const int m = m_nextWindowIndex[session->clientIndex()]++;
		const qint64 tabId = m_nextTabId++;
		const QString title = makeTitle(session->clientIndex(), m);
		session->requestCreateSubWindow(tabId, title);
	}

	void ShellApp::onSubWindowAdded(ClientSession* session, qint64 tabId, QString title, quintptr wid)
	{
		ShellWindow* shell = m_pendingFirstShell.take(session);
		if (!shell)
		{
			// Additional window without pending: prefer shell that already hosts this session
			// (last match — closer to tear-out target than shells_.front()).
			for (auto& s : m_shells)
			{
				for (const auto& t : s->tabs())
				{
					if (t.session == session)
					{
						shell = s.get();
					}
				}
			}
		}
		if (!shell && !m_shells.empty())
		{
			shell = m_shells.back().get();
		}
		if (!shell)
		{
			return;
		}
		TabInfo info;
		info.pageId = session->pageId();
		info.tabId = tabId;
		info.clientIndex = session->clientIndex();
		info.title = title;
		info.wid = wid;
		info.session = session;
		// parse window index from title ClientN-WindowM
		const auto parts = title.split(QLatin1Char('-'));
		if (parts.size() == 2 && parts[1].startsWith(QStringLiteral("Window")))
		{
			info.windowIndex = parts[1].mid(6).toInt();
		}
		m_tabToShell.insert(tabId, shell);
		shell->addTab(info);
		session->notifyReattachment(shell->shellId());
	}

	void ShellApp::onSubWindowRemoved(ClientSession* session, qint64 tabId)
	{
		Q_UNUSED(session);
		if (auto* shell = m_tabToShell.take(tabId))
		{
			shell->removeTab(tabId);
			destroyShellIfEmpty(shell);
		}
	}

	void ShellApp::onSessionDead(ClientSession* session)
	{
		if (!session)
		{
			return;
		}
		// Disconnect further death signals — process finished + socket disconnect both fire.
		session->disconnect(this);

		QList<qint64> tabs;
		for (auto& shell : m_shells)
		{
			if (!shell)
			{
				continue;
			}
			const auto copy = shell->tabs();
			for (const auto& t : copy)
			{
				if (t.session == session)
				{
					tabs.push_back(t.tabId);
				}
			}
		}
		for (qint64 id : tabs)
		{
			if (auto* shell = m_tabToShell.take(id))
			{
				shell->releaseEmbedOwnershipForTab(id);
				shell->removeTab(id);
				destroyShellIfEmpty(shell);
			}
		}
		m_pendingFirstShell.remove(session);
		m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(),
									   [&](const std::unique_ptr<ClientSession>& p)
									   {
										   return p.get() == session;
									   }),
						m_sessions.end());
	}

	void ShellApp::closeShell(ShellWindow* shell)
	{
		if (!shell)
		{
			return;
		}
		const auto tabs = shell->tabs();
		for (const auto& t : tabs)
		{
			if (t.isHome)
			{
				continue;
			}
			m_tabToShell.remove(t.tabId);
			shell->releaseEmbedOwnershipForTab(t.tabId);
			shell->removeTab(t.tabId);
			if (t.session)
			{
				t.session->requestClose(t.tabId);
			}
		}

		// Drop from ownership list before force-close.
		for (auto it = m_shells.begin(); it != m_shells.end(); ++it)
		{
			if (it->get() == shell)
			{
				ShellWindow* raw = it->release();
				m_shells.erase(it);
				raw->forceClose();
				raw->deleteLater();
				break;
			}
		}

		if (m_shells.empty())
		{
			QCoreApplication::quit();
		}
	}

	void ShellApp::closeTab(qint64 tabId)
	{
		if (tabId == kHomeTabId)
		{
			return;
		}
		auto* shell = m_tabToShell.value(tabId, nullptr);
		if (!shell)
		{
			return;
		}
		ClientSession* session = nullptr;
		for (const auto& t : shell->tabs())
		{
			if (t.tabId == tabId)
			{
				session = t.session;
				break;
			}
		}
		if (session)
		{
			session->requestClose(tabId);
		}
		else
		{
			// Orphan tab (no session): drop locally.
			m_tabToShell.remove(tabId);
			shell->removeTab(tabId);
			destroyShellIfEmpty(shell);
		}
	}

	void ShellApp::activateTab(ShellWindow* shell, qint64 tabId)
	{
		shell->setActiveTab(tabId);
	}

	void ShellApp::tearOutTab(ShellWindow* source, qint64 tabId, QRect suggestedGeometry)
	{
		if (!source || tabId == kHomeTabId)
		{
			return;
		}
		TabInfo moved;
		bool found = false;
		for (const auto& t : source->tabs())
		{
			if (t.tabId == tabId)
			{
				moved = t;
				found = true;
				break;
			}
		}
		if (!found || moved.isHome)
		{
			return;
		}
		clearAllDropIndicators();
		clearAllTabYieldPreviews();
		// Keep HWND visible for reparent; preview still covers the transition.
		if (source->embed() && source->embed()->foreignWindow() == moved.wid)
		{
			source->embed()->releaseForeignWindow();
		}
		else
		{
			source->releaseEmbedOwnershipForTab(tabId);
		}
		source->removeTab(tabId);
		m_tabToShell.remove(tabId);

		QPoint pos = suggestedGeometry.topLeft();
		if (pos.isNull())
		{
			pos = QCursor::pos() - QPoint(40, 20);
		}
		QSize sz = suggestedGeometry.size();
		if (!sz.isValid() || sz.width() < 200 || sz.height() < 150)
		{
			sz = m_dragPreviewSize.isValid() ? m_dragPreviewSize : QSize(960, 640);
		}

		// Create hidden, embed first, then show — preview stays on top until first paints.
		auto* neu = createShell(pos, sz, /*showNow=*/false);
		m_tabToShell.insert(tabId, neu);
		neu->addTab(moved);
		if (moved.session)
		{
			moved.session->notifyReattachment(neu->shellId());
			moved.session->requestActivate(tabId);
		}
		if (neu->embed() && moved.wid)
		{
			neu->embed()->setForeignWindow(moved.wid);
			neu->embed()->resyncForeignWindow();
		}
		if (m_tearOutPreview)
		{
			m_tearOutPreview->setGeometry(QRect(pos, sz));
			if (!m_tearOutPreview->isVisible())
			{
				m_tearOutPreview->show();
			}
			m_tearOutPreview->raise();
		}
		if (m_tabDragGhost)
		{
			m_tabDragGhost->hide();
		}
		neu->show();
		neu->raise();
		neu->activateWindow();
		if (neu->embed())
		{
			neu->embed()->resyncForeignWindow();
			QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		// Keep the preview covering the new shell briefly so the first embed paint
		// happens underneath (reduces black/empty flash).
		QTimer::singleShot(48, this,
						   [this]()
						   {
							   if (m_dragActive)
							   {
								   return;
							   }
							   if (m_tearOutPreview)
							   {
								   m_tearOutPreview->hide();
								   m_tearOutPreview->setContentPixmap({});
							   }
							   if (m_tabDragGhost)
							   {
								   m_tabDragGhost->setPixmap({});
							   }
						   });
		destroyShellIfEmpty(source);
	}

	void ShellApp::mergeTab(qint64 tabId, ShellWindow* target, int insertIndex)
	{
		if (!target)
		{
			return;
		}
		auto* source = m_tabToShell.value(tabId, nullptr);
		if (!tab_strip::canMergeTab(tabId, tabId == kHomeTabId, source != nullptr, source == target))
		{
			return;
		}
		TabInfo moved;
		for (const auto& t : source->tabs())
		{
			if (t.tabId == tabId)
			{
				moved = t;
				break;
			}
		}
		if (!moved.tabId || moved.isHome)
		{
			return;
		}
		clearAllDropIndicators();
		// Detach from source embed without Hide — target will reparent immediately.
		if (source->embed() && source->embed()->foreignWindow() == moved.wid)
		{
			source->embed()->releaseForeignWindow();
		}
		else
		{
			source->releaseEmbedOwnershipForTab(tabId);
		}
		source->removeTab(tabId);
		m_tabToShell.insert(tabId, target);
		if (insertIndex < 0)
		{
			target->addTab(moved);
		}
		else
		{
			target->insertTab(moved, insertIndex);
		}
		if (moved.session)
		{
			moved.session->notifyReattachment(target->shellId());
			moved.session->requestActivate(tabId);
		}
		// Force HWND into the target embed (content must follow the tab).
		if (target->embed() && moved.wid)
		{
			target->embed()->setForeignWindow(moved.wid);
			target->embed()->resyncForeignWindow();
		}
		target->raise();
		target->activateWindow();
		destroyShellIfEmpty(source);
	}

	ShellWindow* ShellApp::shellForTab(qint64 tabId) const
	{
		return m_tabToShell.value(tabId, nullptr);
	}

	ShellWindow* ShellApp::shellAtGlobal(QPoint globalPos) const
	{
		for (const auto& shell : m_shells)
		{
			if (shell && shell->isVisible() && shell->frameGeometry().contains(globalPos))
			{
				return shell.get();
			}
		}
		return nullptr;
	}

	ShellWindow* ShellApp::tabDropZoneShellAtGlobal(QPoint globalPos) const
	{
		for (const auto& shell : m_shells)
		{
			if (shell && shell->isVisible() && shell->isOverTabDropZone(globalPos))
			{
				return shell.get();
			}
		}
		return nullptr;
	}

	bool ShellApp::shouldSuppressTearOutAt(QPoint globalPos) const
	{
		const bool overAny = tabDropZoneShellAtGlobal(globalPos) != nullptr;
		const bool nearLeave =
			m_dragSource
			&& m_dragSource->isNearTabDropZone(globalPos, tab_strip::kTearOutLeaveSlopV, tab_strip::kTearOutLeaveSlopH);
		return tab_strip::shouldSuppressTearOut(overAny, nearLeave);
	}

	void ShellApp::beginTabDrag(ShellWindow* source, qint64 tabId, QPoint localHotSpot)
	{
		if (!source || tabId == kHomeTabId)
		{
			return;
		}
		m_dragActive = true;
		m_dragDropHandled = false;
		m_dragCancelled = false;
		m_tearOutDetached = false;
		m_ghostSnapBackActive = false;
		m_dragForbiddenCursor = false;
		m_dragSource = source;
		m_dragTabId = tabId;
		m_dragResumeTabId = 0;
		m_dragPreviewSize = source->size();
		m_dragTabWidth = 0;
#ifdef Q_OS_WIN
		// Clear Esc transition bit so a prior Esc press is not mistaken for cancel.
		GetAsyncKeyState(VK_ESCAPE);
#endif

		quintptr wid = 0;
		for (const auto& t : source->tabs())
		{
			if (t.tabId == tabId)
			{
				wid = t.wid;
				if (t.session)
				{
					t.session->setDragSuppress(true);
				}
				break;
			}
		}

		// Snapshot tab face + content BEFORE hiding / switching away.
		const QSize tabLogicalSize = source->tabButtonSize(tabId);
		m_dragTabWidth = tabLogicalSize.width() > 0 ? tabLogicalSize.width() : 80;
		const QPixmap tabGhostPm = source->grabTabButton(tabId);
		QPixmap contentSnap;
		if (source->activeTabId() == tabId && source->embed())
		{
			contentSnap = source->embed()->grab();
		}
		if (contentSnap.isNull() && wid)
		{
			contentSnap = captureWindowPixmap(wid, m_dragPreviewSize);
		}

		source->setTabDragHidden(tabId, true);

		// While dragging, show the previous tab's content in the source shell.
		if (source->activeTabId() == tabId)
		{
			m_dragResumeTabId = tabId;
			const qint64 next = source->previousActivationTarget(tabId);
			if (next != tabId)
			{
				source->setActiveTab(next);
			}
		}

		if (m_tabDragGhost)
		{
			const QSize contentSz = tabLogicalSize.isValid() ? tabLogicalSize : QSize(m_dragTabWidth, 28);
			m_tabDragGhost->setTabPixmap(tabGhostPm, contentSz);
			// Press-point hotspot: keep grab point under the cursor while free-
			// following. Strip mode still pins content top to the tab row (see below).
			const QPoint origin = m_tabDragGhost->contentOrigin();
			int hx = localHotSpot.x();
			int hy = localHotSpot.y();
			if (hx <= 0)
			{
				hx = contentSz.width() / 2;
			}
			if (hy <= 0)
			{
				hy = contentSz.height() / 2;
			}
			hx = qBound(4, hx, qMax(4, contentSz.width() - 4));
			hy = qBound(2, hy, qMax(2, contentSz.height() - 2));
			m_tabGhostHotSpot = QPoint(origin.x() + hx, origin.y() + hy);
			m_tabDragGhost->hide();
		}
		// Fallback hotspot if geometry must be estimated without a visible ghost.
		const int previewHx =
			qBound(16,
				   int(double(m_tabGhostHotSpot.x() - (m_tabDragGhost ? m_tabDragGhost->contentOrigin().x() : 0))
					   * double(m_dragPreviewSize.width()) / qMax(1, m_dragTabWidth)),
				   m_dragPreviewSize.width() - 16);
		m_dragHotSpot = QPoint(previewHx, TearOutPreview::kFramePad + TearOutPreview::kTitleBarHeight / 2);

		if (m_tearOutPreview)
		{
			m_tearOutPreview->setContentPixmap(contentSnap);
			m_tearOutPreview->resize(m_dragPreviewSize);
			m_tearOutPreview->hide();
		}
		qApp->installEventFilter(this);
		if (m_dragVisualTimer)
		{
			m_dragVisualTimer->start();
		}
		updateTabDragVisuals();
	}

	void ShellApp::noteTabDragDropHandled()
	{
		m_dragDropHandled = true;
	}

	bool ShellApp::consumeDragCancelled()
	{
		pollEscapeCancel();
		const bool cancelled = m_dragCancelled;
		m_dragCancelled = false;
		return cancelled;
	}

	void ShellApp::pollEscapeCancel()
	{
#ifdef Q_OS_WIN
		// Windows OLE DoDragDrop often never delivers Qt KeyPress for Esc.
		const SHORT esc = GetAsyncKeyState(VK_ESCAPE);
		if ((esc & 0x8000) || (esc & 0x0001))
		{
			if (!m_dragCancelled)
			{
				m_dragCancelled = true;
				startGhostSnapBack();
			}
		}
#endif
	}

	void ShellApp::startGhostSnapBack()
	{
		if (!m_tabDragGhost || !m_dragSource || m_dragTabId == 0)
		{
			finishGhostSnapBack();
			return;
		}
		if (m_tearOutPreview)
		{
			m_tearOutPreview->hide();
		}
		m_tearOutDetached = false;

		QRect target = m_dragSource->tabDragSlotGlobalRect(m_dragTabId);
		if (!target.isValid())
		{
			finishGhostSnapBack();
			return;
		}
		// Keep ghost size; snap content into the slot (account for shadow pad).
		const QPoint origin = m_tabDragGhost->contentOrigin();
		const QSize ghostSize = m_tabDragGhost->size();
		const QRect end(target.topLeft() - origin, ghostSize);
		const QRect start = m_tabDragGhost->geometry();

		m_ghostSnapBackActive = true;
		if (!m_tabDragGhost->isVisible())
		{
			m_tabDragGhost->show();
		}
		m_tabDragGhost->raise();

		if (!m_ghostSnapAnim)
		{
			m_ghostSnapAnim = new QPropertyAnimation(m_tabDragGhost, "geometry", this);
			m_ghostSnapAnim->setDuration(160);
			m_ghostSnapAnim->setEasingCurve(QEasingCurve::InOutCubic);
			connect(m_ghostSnapAnim, &QPropertyAnimation::finished, this, &ShellApp::finishGhostSnapBack);
		}
		m_ghostSnapAnim->stop();
		m_ghostSnapAnim->setStartValue(start);
		m_ghostSnapAnim->setEndValue(end);
		m_ghostSnapAnim->start();
	}

	void ShellApp::finishGhostSnapBack()
	{
		m_ghostSnapBackActive = false;
		if (m_tabDragGhost)
		{
			m_tabDragGhost->hide();
		}
		// Keep source yield until endTabDrag so slot stays stable during anim; clear others.
		for (auto& s : m_shells)
		{
			if (s && s.get() != m_dragSource)
			{
				s->clearTabYieldPreview();
				s->clearDropInsertIndicator();
			}
		}
	}

	QRect ShellApp::tearOutPreviewGeometry() const
	{
		if (m_tearOutPreview && m_tearOutPreview->isVisible())
		{
			return m_tearOutPreview->geometry();
		}
		// Fallback: same wrap math around the tab ghost (or cursor).
		if (m_tabDragGhost && m_tabDragGhost->isVisible())
		{
			const QPoint o = m_tabDragGhost->contentOrigin();
			const QRect tabContent(m_tabDragGhost->pos() + o, m_tabDragGhost->contentSize());
			const QRect geo = TearOutPreview::geometryForTabContent(tabContent, m_dragPreviewSize);
			if (geo.isValid())
			{
				return geo;
			}
		}
		const QPoint pos = QCursor::pos() - m_dragHotSpot;
		return QRect(pos, m_dragPreviewSize);
	}

	void ShellApp::endTabDrag(bool tearOrMerge)
	{
		if (!m_dragActive)
		{
			return;
		}
		qApp->removeEventFilter(this);
		if (m_dragVisualTimer)
		{
			m_dragVisualTimer->stop();
		}
		if (m_ghostSnapAnim)
		{
			m_ghostSnapAnim->stop();
		}
		m_ghostSnapBackActive = false;
		m_tearOutDetached = false;
		// Keep preview visible across tear-out until the new shell is shown.
		if (!tearOrMerge)
		{
			if (m_tearOutPreview)
			{
				m_tearOutPreview->hide();
				m_tearOutPreview->setContentPixmap({});
			}
			if (m_tabDragGhost)
			{
				m_tabDragGhost->hide();
				m_tabDragGhost->setPixmap({});
			}
		}
		else if (m_tabDragGhost)
		{
			// Tear-out uses the window preview; hide the tab ghost.
			m_tabDragGhost->hide();
		}
		clearAllTabYieldPreviews();

		ShellWindow* source = m_dragSource;
		const qint64 tabId = m_dragTabId;
		const qint64 resumeId = m_dragResumeTabId;
		const bool dropHandled = m_dragDropHandled;

		m_dragActive = false;
		m_dragSource = nullptr;
		m_dragTabId = 0;
		m_dragResumeTabId = 0;
		m_dragDropHandled = false;
		m_dragTabWidth = 0;

		if (!source)
		{
			return;
		}

		ClientSession* session = nullptr;
		bool stillHere = false;
		for (const auto& t : source->tabs())
		{
			if (t.tabId == tabId)
			{
				stillHere = true;
				session = t.session;
				break;
			}
		}
		if (session)
		{
			session->setDragSuppress(false);
		}

		if (stillHere && !tearOrMerge)
		{
			source->setTabDragHidden(tabId, false);
			// Reorder already activated the tab; cancel/no-move → restore dragged tab.
			if (!dropHandled && resumeId == tabId)
			{
				source->setActiveTab(tabId);
			}
		}
		clearAllDropIndicators();
	}

	void ShellApp::updateTabDragVisuals()
	{
		if (!m_dragActive)
		{
			return;
		}
		pollEscapeCancel();
		if (m_ghostSnapBackActive)
		{
			return;
		}
		if (m_dragCancelled)
		{
			if (m_tearOutPreview)
			{
				m_tearOutPreview->hide();
			}
			clearAllDropIndicators();
			return;
		}
		const QPoint g = QCursor::pos();

		// Forbidden cursor over window min/max/close (not a drop target).
		bool forbidden = false;
		for (auto& s : m_shells)
		{
			if (s && s->isOverWindowButtons(g))
			{
				forbidden = true;
				break;
			}
		}
		if (forbidden != m_dragForbiddenCursor)
		{
			m_dragForbiddenCursor = forbidden;
			if (QApplication::overrideCursor())
			{
				QApplication::changeOverrideCursor(forbidden ? Qt::ForbiddenCursor : Qt::ArrowCursor);
			}
		}

		const bool overStrip = tabDropZoneShellAtGlobal(g) != nullptr;
		const bool nearLeave =
			m_dragSource
			&& m_dragSource->isNearTabDropZone(g, tab_strip::kTearOutLeaveSlopV, tab_strip::kTearOutLeaveSlopH);
		const bool nearReturn =
			m_dragSource
			&& m_dragSource->isNearTabDropZone(g, tab_strip::kTearOutReturnSlopV, tab_strip::kTearOutReturnSlopH);

		const bool wasDetached = m_tearOutDetached;
		m_tearOutDetached = tab_strip::nextTearOutDetached(wasDetached, overStrip, nearLeave, nearReturn);

		const int contentHotX = m_tabGhostHotSpot.x() - (m_tabDragGhost ? m_tabDragGhost->contentOrigin().x() : 0);

		// The tab always stays under the cursor. Window preview is an extra layer
		// while detached — never replace/hide the tab ghost for it.
		auto positionTabGhost = [&](bool pinToStrip, ShellWindow* stripShell, bool bumpZ)
		{
			if (!m_tabDragGhost)
			{
				return;
			}
			const QPoint origin = m_tabDragGhost->contentOrigin();
			const int contentW = m_tabDragGhost->contentSize().width();
			int left = g.x() - m_tabGhostHotSpot.x();
			// Free-follow: press point stays under the cursor (avoids downward bias).
			int top = g.y() - m_tabGhostHotSpot.y();
			if (pinToStrip && stripShell)
			{
				// On strip: lock content top to the tab row; X still follows.
				top = stripShell->tabRowTopGlobal() - origin.y();
				const QRect band = stripShell->tabStripGlobalRect();
				if (band.isValid())
				{
					const int contentLeft = g.x() - contentHotX;
					const int clampedContentLeft = qBound(band.left(), contentLeft, band.right() - contentW);
					left = clampedContentLeft - origin.x();
				}
			}
			if (m_tabDragGhost->pos() != QPoint(left, top))
			{
				m_tabDragGhost->move(left, top);
			}
			const bool needShow = !m_tabDragGhost->isVisible();
			if (needShow)
			{
				m_tabDragGhost->show();
			}
			// Raising every frame fights the window preview and flickers on Win/DPI.
			if (bumpZ || needShow)
			{
				m_tabDragGhost->raise();
			}
		};

		if (!m_tearOutDetached)
		{
			if (wasDetached && m_tearOutPreview)
			{
				m_tearOutPreview->hide();
			}
			ShellWindow* stripShell = tabDropZoneShellAtGlobal(g);
			if (!stripShell)
			{
				stripShell = m_dragSource;
			}
			// Pin Y only while the cursor is actually on the strip. During the leave
			// slop (cursor already below the strip, window preview not yet shown) the
			// tab must free-follow — otherwise it stays glued high while the pointer
			// moves down and looks upwardly biased.
			positionTabGhost(/*pinToStrip=*/overStrip, stripShell, /*bumpZ=*/wasDetached);

			if (m_dragSource && m_dragTabId != 0)
			{
				if (!stripShell && nearLeave)
				{
					stripShell = m_dragSource;
				}
				const int guestW =
					m_dragTabWidth > 0 ? m_dragTabWidth : (m_tabDragGhost ? m_tabDragGhost->contentSize().width() : 80);
				if (stripShell == m_dragSource)
				{
					for (auto& s : m_shells)
					{
						if (s && s.get() != m_dragSource)
						{
							s->clearDropInsertIndicator();
							s->clearTabYieldPreview();
						}
					}
					m_dragSource->previewTabYieldAtCursor(m_dragTabId, g, 0, contentHotX);
				}
				else if (stripShell)
				{
					m_dragSource->clearTabYieldPreview();
					stripShell->clearDropInsertIndicator();
					stripShell->previewTabYieldAtCursor(m_dragTabId, g, guestW, contentHotX);
				}
			}
			return;
		}

		// Detached: tab ghost follows the cursor; window preview is placed so its
		// title/tab bar wraps (vertically centers) that tab — not an independent hotspot.
		if (!wasDetached)
		{
			clearAllDropIndicators();
			// As soon as the tear-out window appears, siblings claim the old slot.
			if (m_dragSource && m_dragTabId != 0)
			{
				m_dragSource->collapseTornOutTabSlot(m_dragTabId);
			}
			for (auto& s : m_shells)
			{
				if (s && s.get() != m_dragSource)
				{
					s->clearTabYieldPreview();
				}
			}
		}
		positionTabGhost(/*pinToStrip=*/false, nullptr, /*bumpZ=*/!wasDetached);
		if (m_tearOutPreview && m_tabDragGhost)
		{
			const QPoint o = m_tabDragGhost->contentOrigin();
			const QRect tabContent(m_tabDragGhost->pos() + o, m_tabDragGhost->contentSize());
			const bool previewNeedShow = !m_tearOutPreview->isVisible();
			m_tearOutPreview->alignToTabContent(tabContent);
			if (previewNeedShow || !wasDetached)
			{
				m_tearOutPreview->raise();
				m_tabDragGhost->raise();
			}
		}
	}

	void ShellApp::destroyShellIfEmpty(ShellWindow* shell)
	{
		if (!shell)
		{
			return;
		}
		if (!tab_strip::shouldDestroyEmptyShell(shell->clientTabCount(), static_cast<int>(m_shells.size())))
		{
			if (shell->clientTabCount() == 0)
			{
				shell->setActiveTab(kHomeTabId);
			}
			return;
		}
		shell->setActiveTab(kHomeTabId);
		for (auto it = m_shells.begin(); it != m_shells.end(); ++it)
		{
			if (it->get() == shell)
			{
				ShellWindow* raw = it->release();
				m_shells.erase(it);
				raw->forceClose();
				raw->deleteLater();
				return;
			}
		}
	}

	QString ShellApp::makeTitle(int clientIndex, int windowIndex) const
	{
		return QStringLiteral("Client%1-Window%2").arg(clientIndex).arg(windowIndex);
	}
} // namespace mps::host
