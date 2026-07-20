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
		, clientExe_(std::move(clientExe))
	{
		token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
		endpoint_ = QStringLiteral("mps-demo-%1").arg(token_);
		server_ = new QLocalServer(this);
		QLocalServer::removeServer(endpoint_);
		if (!server_->listen(endpoint_))
		{
			qWarning("Failed to listen on %s", qPrintable(endpoint_));
		}
		connect(server_, &QLocalServer::newConnection, this, &ShellApp::onNewConnection);

		// Tool windows: no QWidget parent (ShellApp is QObject-only). Owned explicitly.
		tearOutPreview_ = new TearOutPreview(nullptr);
		tearOutPreview_->hide();
		tabDragGhost_ = new TabDragGhost(nullptr);
		tabDragGhost_->hide();
		dragVisualTimer_ = new QTimer(this);
		dragVisualTimer_->setInterval(16);
		connect(dragVisualTimer_, &QTimer::timeout, this, &ShellApp::updateTabDragVisuals);
	}

	ShellApp::~ShellApp()
	{
		delete tearOutPreview_;
		tearOutPreview_ = nullptr;
		delete tabDragGhost_;
		tabDragGhost_ = nullptr;
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
		shells_.push_back(std::move(shell));
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
		for (auto& shell : shells_)
		{
			if (shell)
			{
				shell->clearDropInsertIndicator();
			}
		}
	}

	void ShellApp::clearAllTabYieldPreviews()
	{
		for (auto& shell : shells_)
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
		for (const auto& shell : shells_)
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
		if (dragActive_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride))
		{
			auto* ke = static_cast<QKeyEvent*>(event);
			if (ke->key() == Qt::Key_Escape)
			{
				if (!dragCancelled_)
				{
					dragCancelled_ = true;
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
				dragTabWidth_ > 0 ? dragTabWidth_ : (tabDragGhost_ ? tabDragGhost_->contentSize().width() : 80);

			// Only one shell shows strip feedback at a time.
			for (auto& s : shells_)
			{
				if (!s || s.get() == shell)
				{
					continue;
				}
				s->clearDropInsertIndicator();
				s->clearTabYieldPreview();
			}

			if (dragSource_ && shell == dragSource_ && tabId == dragTabId_)
			{
				shell->previewTabYieldAtCursor(tabId, QCursor::pos(), 0,
											   tabGhostHotSpot_.x()
												   - (tabDragGhost_ ? tabDragGhost_->contentOrigin().x() : 0));
			}
			else
			{
				if (dragSource_ && dragSource_ != shell)
				{
					dragSource_->clearTabYieldPreview();
				}
				// Merge target: live tab yield, not only a blue bar.
				shell->clearDropInsertIndicator();
				shell->previewTabYieldAtCursor(tabId, dropGlobal, guestW,
											   tabGhostHotSpot_.x()
												   - (tabDragGhost_ ? tabDragGhost_->contentOrigin().x() : 0));
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
		const int clientIndex = nextClientIndex_++;
		nextWindowIndex_[clientIndex] = 1;
		auto session = std::make_unique<ClientSession>(clientIndex, endpoint_, this);
		auto* raw = session.get();
		pendingFirstShell_.insert(raw, shell);
		connect(raw, &ClientSession::sessionHelloOk, this, &ShellApp::onSessionReady);
		connect(raw, &ClientSession::subWindowAdded, this, &ShellApp::onSubWindowAdded);
		connect(raw, &ClientSession::subWindowRemoved, this, &ShellApp::onSubWindowRemoved);
		connect(raw, &ClientSession::sessionDead, this, &ShellApp::onSessionDead);
		connect(raw, &ClientSession::invokeNewWindow, this,
				[this](ClientSession* session, qint64 sourceTabId)
				{
					const int m = nextWindowIndex_[session->clientIndex()]++;
					const qint64 tabId = nextTabId_++;
					const QString title = makeTitle(session->clientIndex(), m);
					// Prefer the shell that hosts the page where the user clicked.
					ShellWindow* shell = shellForTab(sourceTabId);
					if (!shell)
					{
						// Fallback: any shell that already hosts this session (prefer last match).
						for (auto& s : shells_)
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
					if (!shell && !shells_.empty())
					{
						shell = shells_.back().get();
					}
					if (shell)
					{
						pendingFirstShell_.insert(session, shell);
					}
					session->requestCreateSubWindow(tabId, title);
				});
		raw->startClientProcess(clientExe_, token_);
		sessions_.push_back(std::move(session));
	}

	void ShellApp::onNewConnection()
	{
		while (server_->hasPendingConnections())
		{
			auto* sock = server_->nextPendingConnection();
			// Attach to the newest session still without a socket.
			ClientSession* target = nullptr;
			for (auto it = sessions_.rbegin(); it != sessions_.rend(); ++it)
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
		auto* shell = pendingFirstShell_.value(session, nullptr);
		if (!shell)
		{
			return;
		}
		const int m = nextWindowIndex_[session->clientIndex()]++;
		const qint64 tabId = nextTabId_++;
		const QString title = makeTitle(session->clientIndex(), m);
		session->requestCreateSubWindow(tabId, title);
	}

	void ShellApp::onSubWindowAdded(ClientSession* session, qint64 tabId, QString title, quintptr wid)
	{
		ShellWindow* shell = pendingFirstShell_.take(session);
		if (!shell)
		{
			// Additional window without pending: prefer shell that already hosts this session
			// (last match — closer to tear-out target than shells_.front()).
			for (auto& s : shells_)
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
		if (!shell && !shells_.empty())
		{
			shell = shells_.back().get();
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
		tabToShell_.insert(tabId, shell);
		shell->addTab(info);
		session->notifyReattachment(shell->shellId());
	}

	void ShellApp::onSubWindowRemoved(ClientSession* session, qint64 tabId)
	{
		Q_UNUSED(session);
		if (auto* shell = tabToShell_.take(tabId))
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
		for (auto& shell : shells_)
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
			if (auto* shell = tabToShell_.take(id))
			{
				shell->releaseEmbedOwnershipForTab(id);
				shell->removeTab(id);
				destroyShellIfEmpty(shell);
			}
		}
		pendingFirstShell_.remove(session);
		sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
									   [&](const std::unique_ptr<ClientSession>& p)
									   {
										   return p.get() == session;
									   }),
						sessions_.end());
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
			tabToShell_.remove(t.tabId);
			shell->releaseEmbedOwnershipForTab(t.tabId);
			shell->removeTab(t.tabId);
			if (t.session)
			{
				t.session->requestClose(t.tabId);
			}
		}

		// Drop from ownership list before force-close.
		for (auto it = shells_.begin(); it != shells_.end(); ++it)
		{
			if (it->get() == shell)
			{
				ShellWindow* raw = it->release();
				shells_.erase(it);
				raw->forceClose();
				raw->deleteLater();
				break;
			}
		}

		if (shells_.empty())
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
		auto* shell = tabToShell_.value(tabId, nullptr);
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
			tabToShell_.remove(tabId);
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
		tabToShell_.remove(tabId);

		QPoint pos = suggestedGeometry.topLeft();
		if (pos.isNull())
		{
			pos = QCursor::pos() - QPoint(40, 20);
		}
		QSize sz = suggestedGeometry.size();
		if (!sz.isValid() || sz.width() < 200 || sz.height() < 150)
		{
			sz = dragPreviewSize_.isValid() ? dragPreviewSize_ : QSize(960, 640);
		}

		// Create hidden, embed first, then show — preview stays on top until first paints.
		auto* neu = createShell(pos, sz, /*showNow=*/false);
		tabToShell_.insert(tabId, neu);
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
		if (tearOutPreview_)
		{
			tearOutPreview_->setGeometry(QRect(pos, sz));
			if (!tearOutPreview_->isVisible())
			{
				tearOutPreview_->show();
			}
			tearOutPreview_->raise();
		}
		if (tabDragGhost_)
		{
			tabDragGhost_->hide();
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
							   if (dragActive_)
							   {
								   return;
							   }
							   if (tearOutPreview_)
							   {
								   tearOutPreview_->hide();
								   tearOutPreview_->setContentPixmap({});
							   }
							   if (tabDragGhost_)
							   {
								   tabDragGhost_->setPixmap({});
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
		auto* source = tabToShell_.value(tabId, nullptr);
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
		tabToShell_.insert(tabId, target);
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
		return tabToShell_.value(tabId, nullptr);
	}

	ShellWindow* ShellApp::shellAtGlobal(QPoint globalPos) const
	{
		for (const auto& shell : shells_)
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
		for (const auto& shell : shells_)
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
			dragSource_
			&& dragSource_->isNearTabDropZone(globalPos, tab_strip::kTearOutLeaveSlopV, tab_strip::kTearOutLeaveSlopH);
		return tab_strip::shouldSuppressTearOut(overAny, nearLeave);
	}

	void ShellApp::beginTabDrag(ShellWindow* source, qint64 tabId, QPoint localHotSpot)
	{
		if (!source || tabId == kHomeTabId)
		{
			return;
		}
		dragActive_ = true;
		dragDropHandled_ = false;
		dragCancelled_ = false;
		tearOutDetached_ = false;
		ghostSnapBackActive_ = false;
		dragForbiddenCursor_ = false;
		dragSource_ = source;
		dragTabId_ = tabId;
		dragResumeTabId_ = 0;
		dragPreviewSize_ = source->size();
		dragTabWidth_ = 0;
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
		dragTabWidth_ = tabLogicalSize.width() > 0 ? tabLogicalSize.width() : 80;
		const QPixmap tabGhostPm = source->grabTabButton(tabId);
		QPixmap contentSnap;
		if (source->activeTabId() == tabId && source->embed())
		{
			contentSnap = source->embed()->grab();
		}
		if (contentSnap.isNull() && wid)
		{
			contentSnap = captureWindowPixmap(wid, dragPreviewSize_);
		}

		source->setTabDragHidden(tabId, true);

		// While dragging, show the previous tab's content in the source shell.
		if (source->activeTabId() == tabId)
		{
			dragResumeTabId_ = tabId;
			const qint64 next = source->previousActivationTarget(tabId);
			if (next != tabId)
			{
				source->setActiveTab(next);
			}
		}

		if (tabDragGhost_)
		{
			const QSize contentSz = tabLogicalSize.isValid() ? tabLogicalSize : QSize(dragTabWidth_, 28);
			tabDragGhost_->setTabPixmap(tabGhostPm, contentSz);
			// Press-point hotspot: keep grab point under the cursor while free-
			// following. Strip mode still pins content top to the tab row (see below).
			const QPoint origin = tabDragGhost_->contentOrigin();
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
			tabGhostHotSpot_ = QPoint(origin.x() + hx, origin.y() + hy);
			tabDragGhost_->hide();
		}
		// Fallback hotspot if geometry must be estimated without a visible ghost.
		const int previewHx =
			qBound(16,
				   int(double(tabGhostHotSpot_.x() - (tabDragGhost_ ? tabDragGhost_->contentOrigin().x() : 0))
					   * double(dragPreviewSize_.width()) / qMax(1, dragTabWidth_)),
				   dragPreviewSize_.width() - 16);
		dragHotSpot_ = QPoint(previewHx, TearOutPreview::kFramePad + TearOutPreview::kTitleBarHeight / 2);

		if (tearOutPreview_)
		{
			tearOutPreview_->setContentPixmap(contentSnap);
			tearOutPreview_->resize(dragPreviewSize_);
			tearOutPreview_->hide();
		}
		qApp->installEventFilter(this);
		if (dragVisualTimer_)
		{
			dragVisualTimer_->start();
		}
		updateTabDragVisuals();
	}

	void ShellApp::noteTabDragDropHandled()
	{
		dragDropHandled_ = true;
	}

	bool ShellApp::consumeDragCancelled()
	{
		pollEscapeCancel();
		const bool cancelled = dragCancelled_;
		dragCancelled_ = false;
		return cancelled;
	}

	void ShellApp::pollEscapeCancel()
	{
#ifdef Q_OS_WIN
		// Windows OLE DoDragDrop often never delivers Qt KeyPress for Esc.
		const SHORT esc = GetAsyncKeyState(VK_ESCAPE);
		if ((esc & 0x8000) || (esc & 0x0001))
		{
			if (!dragCancelled_)
			{
				dragCancelled_ = true;
				startGhostSnapBack();
			}
		}
#endif
	}

	void ShellApp::startGhostSnapBack()
	{
		if (!tabDragGhost_ || !dragSource_ || dragTabId_ == 0)
		{
			finishGhostSnapBack();
			return;
		}
		if (tearOutPreview_)
		{
			tearOutPreview_->hide();
		}
		tearOutDetached_ = false;

		QRect target = dragSource_->tabDragSlotGlobalRect(dragTabId_);
		if (!target.isValid())
		{
			finishGhostSnapBack();
			return;
		}
		// Keep ghost size; snap content into the slot (account for shadow pad).
		const QPoint origin = tabDragGhost_->contentOrigin();
		const QSize ghostSize = tabDragGhost_->size();
		const QRect end(target.topLeft() - origin, ghostSize);
		const QRect start = tabDragGhost_->geometry();

		ghostSnapBackActive_ = true;
		if (!tabDragGhost_->isVisible())
		{
			tabDragGhost_->show();
		}
		tabDragGhost_->raise();

		if (!ghostSnapAnim_)
		{
			ghostSnapAnim_ = new QPropertyAnimation(tabDragGhost_, "geometry", this);
			ghostSnapAnim_->setDuration(160);
			ghostSnapAnim_->setEasingCurve(QEasingCurve::InOutCubic);
			connect(ghostSnapAnim_, &QPropertyAnimation::finished, this, &ShellApp::finishGhostSnapBack);
		}
		ghostSnapAnim_->stop();
		ghostSnapAnim_->setStartValue(start);
		ghostSnapAnim_->setEndValue(end);
		ghostSnapAnim_->start();
	}

	void ShellApp::finishGhostSnapBack()
	{
		ghostSnapBackActive_ = false;
		if (tabDragGhost_)
		{
			tabDragGhost_->hide();
		}
		// Keep source yield until endTabDrag so slot stays stable during anim; clear others.
		for (auto& s : shells_)
		{
			if (s && s.get() != dragSource_)
			{
				s->clearTabYieldPreview();
				s->clearDropInsertIndicator();
			}
		}
	}

	QRect ShellApp::tearOutPreviewGeometry() const
	{
		if (tearOutPreview_ && tearOutPreview_->isVisible())
		{
			return tearOutPreview_->geometry();
		}
		// Fallback: same wrap math around the tab ghost (or cursor).
		if (tabDragGhost_ && tabDragGhost_->isVisible())
		{
			const QPoint o = tabDragGhost_->contentOrigin();
			const QRect tabContent(tabDragGhost_->pos() + o, tabDragGhost_->contentSize());
			const QRect geo = TearOutPreview::geometryForTabContent(tabContent, dragPreviewSize_);
			if (geo.isValid())
			{
				return geo;
			}
		}
		const QPoint pos = QCursor::pos() - dragHotSpot_;
		return QRect(pos, dragPreviewSize_);
	}

	void ShellApp::endTabDrag(bool tearOrMerge)
	{
		if (!dragActive_)
		{
			return;
		}
		qApp->removeEventFilter(this);
		if (dragVisualTimer_)
		{
			dragVisualTimer_->stop();
		}
		if (ghostSnapAnim_)
		{
			ghostSnapAnim_->stop();
		}
		ghostSnapBackActive_ = false;
		tearOutDetached_ = false;
		// Keep preview visible across tear-out until the new shell is shown.
		if (!tearOrMerge)
		{
			if (tearOutPreview_)
			{
				tearOutPreview_->hide();
				tearOutPreview_->setContentPixmap({});
			}
			if (tabDragGhost_)
			{
				tabDragGhost_->hide();
				tabDragGhost_->setPixmap({});
			}
		}
		else if (tabDragGhost_)
		{
			// Tear-out uses the window preview; hide the tab ghost.
			tabDragGhost_->hide();
		}
		clearAllTabYieldPreviews();

		ShellWindow* source = dragSource_;
		const qint64 tabId = dragTabId_;
		const qint64 resumeId = dragResumeTabId_;
		const bool dropHandled = dragDropHandled_;

		dragActive_ = false;
		dragSource_ = nullptr;
		dragTabId_ = 0;
		dragResumeTabId_ = 0;
		dragDropHandled_ = false;
		dragTabWidth_ = 0;

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
		if (!dragActive_)
		{
			return;
		}
		pollEscapeCancel();
		if (ghostSnapBackActive_)
		{
			return;
		}
		if (dragCancelled_)
		{
			if (tearOutPreview_)
			{
				tearOutPreview_->hide();
			}
			clearAllDropIndicators();
			return;
		}
		const QPoint g = QCursor::pos();

		// Forbidden cursor over window min/max/close (not a drop target).
		bool forbidden = false;
		for (auto& s : shells_)
		{
			if (s && s->isOverWindowButtons(g))
			{
				forbidden = true;
				break;
			}
		}
		if (forbidden != dragForbiddenCursor_)
		{
			dragForbiddenCursor_ = forbidden;
			if (QApplication::overrideCursor())
			{
				QApplication::changeOverrideCursor(forbidden ? Qt::ForbiddenCursor : Qt::ArrowCursor);
			}
		}

		const bool overStrip = tabDropZoneShellAtGlobal(g) != nullptr;
		const bool nearLeave =
			dragSource_
			&& dragSource_->isNearTabDropZone(g, tab_strip::kTearOutLeaveSlopV, tab_strip::kTearOutLeaveSlopH);
		const bool nearReturn =
			dragSource_
			&& dragSource_->isNearTabDropZone(g, tab_strip::kTearOutReturnSlopV, tab_strip::kTearOutReturnSlopH);

		const bool wasDetached = tearOutDetached_;
		tearOutDetached_ = tab_strip::nextTearOutDetached(wasDetached, overStrip, nearLeave, nearReturn);

		const int contentHotX = tabGhostHotSpot_.x() - (tabDragGhost_ ? tabDragGhost_->contentOrigin().x() : 0);

		// The tab always stays under the cursor. Window preview is an extra layer
		// while detached — never replace/hide the tab ghost for it.
		auto positionTabGhost = [&](bool pinToStrip, ShellWindow* stripShell, bool bumpZ)
		{
			if (!tabDragGhost_)
			{
				return;
			}
			const QPoint origin = tabDragGhost_->contentOrigin();
			const int contentW = tabDragGhost_->contentSize().width();
			int left = g.x() - tabGhostHotSpot_.x();
			// Free-follow: press point stays under the cursor (avoids downward bias).
			int top = g.y() - tabGhostHotSpot_.y();
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
			if (tabDragGhost_->pos() != QPoint(left, top))
			{
				tabDragGhost_->move(left, top);
			}
			const bool needShow = !tabDragGhost_->isVisible();
			if (needShow)
			{
				tabDragGhost_->show();
			}
			// Raising every frame fights the window preview and flickers on Win/DPI.
			if (bumpZ || needShow)
			{
				tabDragGhost_->raise();
			}
		};

		if (!tearOutDetached_)
		{
			if (wasDetached && tearOutPreview_)
			{
				tearOutPreview_->hide();
			}
			ShellWindow* stripShell = tabDropZoneShellAtGlobal(g);
			if (!stripShell)
			{
				stripShell = dragSource_;
			}
			// Pin Y only while the cursor is actually on the strip. During the leave
			// slop (cursor already below the strip, window preview not yet shown) the
			// tab must free-follow — otherwise it stays glued high while the pointer
			// moves down and looks upwardly biased.
			positionTabGhost(/*pinToStrip=*/overStrip, stripShell, /*bumpZ=*/wasDetached);

			if (dragSource_ && dragTabId_ != 0)
			{
				if (!stripShell && nearLeave)
				{
					stripShell = dragSource_;
				}
				const int guestW =
					dragTabWidth_ > 0 ? dragTabWidth_ : (tabDragGhost_ ? tabDragGhost_->contentSize().width() : 80);
				if (stripShell == dragSource_)
				{
					for (auto& s : shells_)
					{
						if (s && s.get() != dragSource_)
						{
							s->clearDropInsertIndicator();
							s->clearTabYieldPreview();
						}
					}
					dragSource_->previewTabYieldAtCursor(dragTabId_, g, 0, contentHotX);
				}
				else if (stripShell)
				{
					dragSource_->clearTabYieldPreview();
					stripShell->clearDropInsertIndicator();
					stripShell->previewTabYieldAtCursor(dragTabId_, g, guestW, contentHotX);
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
			if (dragSource_ && dragTabId_ != 0)
			{
				dragSource_->collapseTornOutTabSlot(dragTabId_);
			}
			for (auto& s : shells_)
			{
				if (s && s.get() != dragSource_)
				{
					s->clearTabYieldPreview();
				}
			}
		}
		positionTabGhost(/*pinToStrip=*/false, nullptr, /*bumpZ=*/!wasDetached);
		if (tearOutPreview_ && tabDragGhost_)
		{
			const QPoint o = tabDragGhost_->contentOrigin();
			const QRect tabContent(tabDragGhost_->pos() + o, tabDragGhost_->contentSize());
			const bool previewNeedShow = !tearOutPreview_->isVisible();
			tearOutPreview_->alignToTabContent(tabContent);
			if (previewNeedShow || !wasDetached)
			{
				tearOutPreview_->raise();
				tabDragGhost_->raise();
			}
		}
	}

	void ShellApp::destroyShellIfEmpty(ShellWindow* shell)
	{
		if (!shell)
		{
			return;
		}
		if (!tab_strip::shouldDestroyEmptyShell(shell->clientTabCount(), static_cast<int>(shells_.size())))
		{
			if (shell->clientTabCount() == 0)
			{
				shell->setActiveTab(kHomeTabId);
			}
			return;
		}
		shell->setActiveTab(kHomeTabId);
		for (auto it = shells_.begin(); it != shells_.end(); ++it)
		{
			if (it->get() == shell)
			{
				ShellWindow* raw = it->release();
				shells_.erase(it);
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
