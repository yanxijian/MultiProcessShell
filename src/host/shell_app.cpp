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
#include <QMetaType>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSize>
#include <QTimer>
#include <QUuid>
#include <QWidget>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
	namespace
	{
		/// Topmost OLE hit target that follows the cursor so Windows never falls back
		/// to the native "no drop" / forbidden cursor over HWND gaps or the desktop.
		class TabDragDropSink final : public QWidget
		{
		public:
			TabDragDropSink()
			{
				setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
				setAttribute(Qt::WA_ShowWithoutActivating);
				setAttribute(Qt::WA_TranslucentBackground);
				setAttribute(Qt::WA_NoSystemBackground);
				setAcceptDrops(true);
				resize(72, 72);
				hide();
			}

			void follow(QPoint global)
			{
				const QPoint tl = global - QPoint(width() / 2, height() / 2);
				if (pos() != tl)
				{
					move(tl);
				}
				if (!isVisible())
				{
					show();
				}
				raise();
			}

		protected:
			void paintEvent(QPaintEvent*) override
			{
				// Nearly invisible but hittable — fully transparent HWNDs often miss OLE hits.
				QPainter p(this);
				p.fillRect(rect(), QColor(0, 0, 0, 1));
			}
		};
	} // namespace

	ShellApp::ShellApp(QString clientExe, QString endpointPrefix, QObject* parent)
		: QObject(parent)
		, m_clientExe(std::move(clientExe))
	{
		qRegisterMetaType<mps::theme::Scheme>();

		m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
		const QString prefix = endpointPrefix.isEmpty() ? QStringLiteral("mps") : endpointPrefix;
		m_endpoint = QStringLiteral("%1-%2").arg(prefix, m_token);
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
		m_dragDropSink = new TabDragDropSink();
		m_dragDropSink->installEventFilter(this);
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
		delete m_dragDropSink;
		m_dragDropSink = nullptr;
	}

	ShellWindow* ShellApp::createShell(QPoint pos, QSize size, bool showNow)
	{
		auto shell = std::make_unique<ShellWindow>(this);
		auto* raw = shell.get();
		raw->setWindowTitle(m_shellWindowTitle);
		if (m_homeContentFactory)
		{
			raw->setHomeContent(m_homeContentFactory(raw));
		}
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
		connect(shell, &ShellWindow::terminateSessionRequested, this, &ShellApp::terminateSession);

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
				if (m_dragAutoMerged)
				{
					return false; // abort OLE only; merge already committed
				}
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
		const bool fromSink = isDragDropSink(watched);
		if (!shell && !fromSink)
		{
			return QObject::eventFilter(watched, event);
		}

		if (event->type() == QEvent::DragLeave)
		{
			// Do not clear yield here: Qt often sends DragLeave immediately before Drop,
			// and clearing would lose the insert index / reopen the gap under the ghost.
			if (shell)
			{
				shell->clearDropInsertIndicator();
			}
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
			const int guestW = m_dragTabWidth > 0 ? m_dragTabWidth : (m_tabDragGhost ? m_tabDragGhost->contentSize().width() : 80);
			const int hotX = m_tabGhostHotSpot.x() - (m_tabDragGhost ? m_tabDragGhost->contentOrigin().x() : 0);

			// Whole-shell tear-out / cursor sink: resolve merge by strip geometry.
			if (fromSink || (m_dragMoveWholeShell && m_tearOutDetached))
			{
				const QPoint g = QCursor::pos();
				// Keep the shell under the cursor on every OLE move (not only the 16ms
				// timer) so the pointer never drifts onto a non-accepting widget.
				if (m_dragMoveWholeShell && m_dragSource && m_tearOutDetached)
				{
					const QPoint topLeft = g - m_dragWindowHotSpot;
					if (m_dragSource->frameGeometry().topLeft() != topLeft)
					{
						m_dragSource->move(topLeft);
					}
				}
				ShellWindow* zone = tabDropZoneShellAtGlobal(g);
				for (auto& s : m_shells)
				{
					if (!s)
					{
						continue;
					}
					if (zone && s.get() == zone)
					{
						continue;
					}
					s->clearDropInsertIndicator();
					s->clearTabYieldPreview();
				}
				if (!zone || zone == m_dragSource)
				{
					// Accept MoveAction (not IgnoreAction): Windows OLE shows the
					// forbidden cursor for ignored drops even with a custom Ignore pixmap.
					de->setDropAction(Qt::MoveAction);
					de->accept();
					return true;
				}
				zone->clearDropInsertIndicator();
				zone->previewTabYieldAtCursor(tabId, dropGlobal, guestW, hotX);
				de->setDropAction(Qt::MoveAction);
				de->accept();
				if (m_dragMoveWholeShell)
				{
					tryCommitMagneticAutoMerge();
				}
				return true;
			}

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
				shell->previewTabYieldAtCursor(tabId, QCursor::pos(), 0, hotX);
			}
			else
			{
				if (m_dragSource && m_dragSource != shell)
				{
					m_dragSource->clearTabYieldPreview();
				}
				// Merge target: live tab yield, not only a blue bar.
				shell->clearDropInsertIndicator();
				shell->previewTabYieldAtCursor(tabId, dropGlobal, guestW, hotX);
				if (m_dragMoveWholeShell)
				{
					tryCommitMagneticAutoMerge();
				}
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
			if (m_dragAutoMerged || m_dragDropHandled)
			{
				de->acceptProposedAction();
				return true;
			}
			const qint64 tabId = de->mimeData()->data(QString::fromUtf8(kTabMimeType)).toLongLong();
			QPoint dropGlobal = QCursor::pos();
			if (auto* w = qobject_cast<QWidget*>(watched))
			{
				dropGlobal = w->mapToGlobal(de->position().toPoint());
			}

			ShellWindow* dropShell = shell;
			if (fromSink || (m_dragMoveWholeShell && m_tearOutDetached))
			{
				ShellWindow* zone = tabDropZoneShellAtGlobal(QCursor::pos());
				if (zone && zone != m_dragSource)
				{
					dropShell = zone;
				}
				else
				{
					const QPoint releasePos = QCursor::pos();
					// Spec §5.2: release over min/max/close → cancel (restore), not keep.
					if (tab_strip::shouldCancelTearOutOverWindowButtons(isReleaseOverWindowButtons(releasePos)))
					{
						clearAllDropIndicators();
						clearAllTabYieldPreviews();
						if (m_dragSource && shellStillAlive(m_dragSource) && m_dragSourceSavedGeometry.isValid())
						{
							m_dragSource->setGeometry(m_dragSourceSavedGeometry);
							m_dragSource->setWindowOpacity(1.0);
						}
						noteTabDragDropHandled();
						de->acceptProposedAction();
						return true;
					}
					// No other shell strip under cursor — keep the moved shell as-is.
					clearAllDropIndicators();
					clearAllTabYieldPreviews();
					noteTabDragDropHandled();
					de->acceptProposedAction();
					return true;
				}
			}

			int insertIndex = dropShell->yieldInsertIndex();
			if (insertIndex < 0)
			{
				insertIndex = dropShell->tabInsertIndexAt(dropGlobal);
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
			if (source == dropShell)
			{
				if (!(dropShell->hasTabYieldPreview() && dropShell->commitTabYieldPreview()))
				{
					clearAllDropIndicators();
					clearAllTabYieldPreviews();
					dropShell->moveTab(tabId, mergeIndex);
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
			mergeTab(tabId, dropShell, mergeIndex);
			noteTabDragDropHandled();
			de->acceptProposedAction();
			return true;
		}
		return QObject::eventFilter(watched, event);
	}

	void ShellApp::registerClientLauncher(const QString& appName, const QString& exePath)
	{
		if (appName.isEmpty() || exePath.isEmpty())
		{
			return;
		}
		m_clientLaunchers.insert(appName, exePath);
	}

	qint64 ShellApp::findTabIdForApp(const QString& appName) const
	{
		ClientSession* session = m_sessionsByAppName.value(appName, nullptr);
		if (!session || session->isDead())
		{
			return 0;
		}
		for (auto it = m_tabToShell.constBegin(); it != m_tabToShell.constEnd(); ++it)
		{
			ShellWindow* shell = it.value();
			if (!shell)
			{
				continue;
			}
			for (const auto& t : shell->tabs())
			{
				if (t.session == session && t.tabId == it.key())
				{
					return t.tabId;
				}
			}
		}
		return 0;
	}

	void ShellApp::invokeOnTab(qint64 tabId, const QString& method, const QByteArray& params)
	{
		if (tabId == 0 || method.isEmpty())
		{
			return;
		}
		ShellWindow* shell = m_tabToShell.value(tabId, nullptr);
		if (!shell)
		{
			return;
		}
		for (const auto& t : shell->tabs())
		{
			if (t.tabId == tabId && t.session && !t.session->isDead())
			{
				t.session->sendInvoke(method, params, tabId);
				return;
			}
		}
	}

	QString ShellApp::resolveClientExe(const QString& appName) const
	{
		if (!appName.isEmpty())
		{
			const auto it = m_clientLaunchers.constFind(appName);
			if (it != m_clientLaunchers.cend() && !it.value().isEmpty())
			{
				return it.value();
			}
		}
		return m_clientExe;
	}

	void ShellApp::createClientOn(ShellWindow* shell)
	{
		createClientOnWithExe(shell, m_clientExe);
	}

	void ShellApp::createClientOn(ShellWindow* shell, const QString& appName)
	{
		if (!appName.isEmpty())
		{
			if (ClientSession* existing = m_sessionsByAppName.value(appName, nullptr))
			{
				if (existing && existing->isDead())
				{
					m_sessionsByAppName.remove(appName);
					m_sessionToAppName.remove(existing);
					existing = nullptr;
				}
				if (existing)
				{
					if (existing->channel())
					{
						requestContentViewOnSession(existing, shell);
						return;
					}
					// Same kind still connecting — avoid a second process / pipe race.
					qWarning("ShellApp: client kind \"%s\" still starting; skip duplicate launch", qPrintable(appName));
					return;
				}
			}
		}
		createClientOnWithExe(shell, resolveClientExe(appName), appName);
	}

	void ShellApp::requestContentViewOnSession(ClientSession* session, ShellWindow* shell)
	{
		if (!session || session->isDead())
		{
			return;
		}
		const int m = m_nextContentIndex[session->instanceIndex()]++;
		const qint64 tabId = m_nextTabId++;
		const QString title = makeTitle(session->instanceIndex(), m);
		if (tab_strip::shouldDeferCreateDuringDrag(m_dragActive))
		{
			m_deferredCreatesDuringDrag.push_back(DeferredCreate{session, tabId, title, shell});
			return;
		}
		if (shell)
		{
			m_pendingFirstShell.insert(session, shell);
		}
		session->requestCreateContentView(tabId, title);
	}

	void ShellApp::createClientOnWithExe(ShellWindow* shell, const QString& clientExe, const QString& appName)
	{
		if (m_clientLaunchInFlight)
		{
			return;
		}
		if (clientExe.isEmpty())
		{
			qWarning("ShellApp: no Client executable to launch");
			return;
		}
		for (const auto& existing : m_sessions)
		{
			if (existing && !existing->isDead() && !existing->channel())
			{
				return;
			}
		}
		m_clientLaunchInFlight = true;
		const int instanceIndex = m_nextInstanceIndex++;
		m_nextContentIndex[instanceIndex] = 1;
		auto session = std::make_unique<ClientSession>(instanceIndex, m_endpoint, m_requestNewContentViewMethod, this);
		auto* raw = session.get();
		if (!appName.isEmpty())
		{
			m_sessionsByAppName.insert(appName, raw);
			m_sessionToAppName.insert(raw, appName);
		}
		m_pendingFirstShell.insert(raw, shell);
		connect(raw, &ClientSession::sessionHelloOk, this, &ShellApp::onSessionHelloOk);
		connect(raw, &ClientSession::contentViewReady, this, &ShellApp::onContentViewReady);
		connect(raw, &ClientSession::contentViewClosed, this, &ShellApp::onContentViewClosed);
		connect(raw, &ClientSession::sessionDead, this, &ShellApp::onSessionDead);
		connect(raw, &ClientSession::sessionUnhealthy, this, &ShellApp::onSessionUnhealthy);
		connect(raw, &ClientSession::sessionHealthy, this, &ShellApp::onSessionHealthy);
		connect(raw, &ClientSession::themeSetRequested, this, &ShellApp::onThemeSetRequested);
		connect(raw, &ClientSession::createContentViewRequested, this,
				[this](ClientSession* session, qint64 sourceTabId)
				{
					const int m = m_nextContentIndex[session->instanceIndex()]++;
					const qint64 tabId = m_nextTabId++;
					const QString title = makeTitle(session->instanceIndex(), m);
					// Prefer the shell that hosts the content where the user clicked.
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
					// Spec S5: do not CreateSubWindow while a tear-out drag is in flight.
					if (tab_strip::shouldDeferCreateDuringDrag(m_dragActive))
					{
						m_deferredCreatesDuringDrag.push_back(DeferredCreate{session, tabId, title, shell});
						return;
					}
					if (shell)
					{
						m_pendingFirstShell.insert(session, shell);
					}
					session->requestCreateContentView(tabId, title);
				});
		raw->startClientProcess(clientExe, m_token);
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

	void ShellApp::onSessionHelloOk(ClientSession* session)
	{
		m_clientLaunchInFlight = false;
		// Push SSOT skin before the first CreateSubWindow so the Client content view is born correct.
		pushThemeToSession(session);
		onSessionReady(session);
	}

	void ShellApp::onSessionReady(ClientSession* session)
	{
		auto* shell = m_pendingFirstShell.value(session, nullptr);
		if (!shell)
		{
			return;
		}
		const int m = m_nextContentIndex[session->instanceIndex()]++;
		const qint64 tabId = m_nextTabId++;
		const QString title = makeTitle(session->instanceIndex(), m);
		session->requestCreateContentView(tabId, title);
	}

	void ShellApp::setScheme(mps::theme::Scheme scheme, ThemeOrigin origin)
	{
		if (scheme != mps::theme::Scheme::Light && scheme != mps::theme::Scheme::Dark)
		{
			scheme = mps::theme::Scheme::Light;
		}
		if (m_scheme == scheme)
		{
			return;
		}
		m_scheme = scheme;
		// Demo applies QTE synchronously first so QPalette matches the new scheme,
		// then chrome stylesheets are rebuilt from the updated palette.
		emit schemeChanged(scheme, origin);
		for (auto& shell : m_shells)
		{
			if (shell)
			{
				shell->applyThemeChrome();
			}
		}
		if (origin != ThemeOrigin::Startup)
		{
			broadcastTheme(scheme);
		}
	}

	void ShellApp::onThemeSetRequested(ClientSession* /*session*/, mps::theme::Scheme scheme)
	{
		setScheme(scheme, ThemeOrigin::ClientRequest);
	}

	void ShellApp::pushThemeToSession(ClientSession* session)
	{
		if (!session)
		{
			return;
		}
		session->pushThemeScheme(mps::theme::toParams(m_scheme));
	}

	void ShellApp::broadcastTheme(mps::theme::Scheme scheme)
	{
		const QByteArray params = mps::theme::toParams(scheme);
		for (auto& session : m_sessions)
		{
			if (session && !session->isDead())
			{
				session->pushThemeScheme(params);
			}
		}
	}

	void ShellApp::onContentViewReady(ClientSession* session, qint64 tabId, QString title, quintptr wid)
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
		info.sessionId = session->sessionId();
		info.tabId = tabId;
		info.instanceIndex = session->instanceIndex();
		info.title = title;
		info.session = session;
		info.unhealthy = session->isUnhealthy();
		// parse content index from title ClientN-TabM
		const auto parts = title.split(QLatin1Char('-'));
		if (parts.size() == 2 && parts[1].startsWith(QStringLiteral("Tab")))
		{
			info.contentIndex = parts[1].mid(3).toInt();
		}
		if (!shell->embedContainer() || !wid)
		{
			session->requestClose(tabId);
			return;
		}
		shell->embedContainer()->bind(tabId, wid);
		if (!shell->embedContainer()->has(tabId))
		{
			// Invalid / dead HWND — do not leave a permanent Home-only client tab.
			session->requestClose(tabId);
			return;
		}
		m_tabToShell.insert(tabId, shell);
		shell->addTab(info);
		session->notifyReattachment(shell->shellId());
		const QString appName = m_sessionToAppName.value(session);
		if (!appName.isEmpty())
		{
			emit appContentViewReady(appName, tabId);
		}
	}

	void ShellApp::onContentViewClosed(ClientSession* session, qint64 tabId)
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
		m_clientLaunchInFlight = false;
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
				shell->releaseEmbedTrackingForTab(id);
				shell->removeTab(id);
				destroyShellIfEmpty(shell);
			}
		}
		m_pendingFirstShell.remove(session);
		m_sessionToAppName.remove(session);
		for (auto it = m_sessionsByAppName.begin(); it != m_sessionsByAppName.end();)
		{
			if (it.value() == session)
			{
				it = m_sessionsByAppName.erase(it);
			}
			else
			{
				++it;
			}
		}
		m_deferredCreatesDuringDrag.erase(std::remove_if(m_deferredCreatesDuringDrag.begin(), m_deferredCreatesDuringDrag.end(),
														 [&](const DeferredCreate& d)
														 {
															 return d.session == session;
														 }),
										  m_deferredCreatesDuringDrag.end());
		m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(),
										[&](const std::unique_ptr<ClientSession>& p)
										{
											return p.get() == session;
										}),
						 m_sessions.end());
	}

	void ShellApp::onSessionUnhealthy(ClientSession* session)
	{
		if (!session)
		{
			return;
		}
		for (auto& shell : m_shells)
		{
			if (shell)
			{
				shell->setSessionUnhealthy(session, true);
			}
		}
	}

	void ShellApp::onSessionHealthy(ClientSession* session)
	{
		if (!session)
		{
			return;
		}
		for (auto& shell : m_shells)
		{
			if (shell)
			{
				shell->setSessionUnhealthy(session, false);
			}
		}
	}

	void ShellApp::terminateSession(ClientSession* session)
	{
		if (!session || session->isDead())
		{
			return;
		}
		session->terminateProcess();
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
			shell->releaseEmbedTrackingForTab(t.tabId);
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
		// Spec §7.2 / Chrome last-tab: sole Client tab already moved the real shell.
		// Do not spawn a second shell or transfer the embed.
		if (tab_strip::shouldMoveWholeShellOnTearOut(source->clientTabCount()))
		{
			clearAllDropIndicators();
			clearAllTabYieldPreviews();
			source->setTabDragHidden(tabId, false);
			source->setActiveTab(tabId);
			source->setWindowOpacity(1.0);
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
			source->raise();
			source->activateWindow();
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
		const quintptr wid = EmbedContainer::transferBinding(source->embedContainer(), nullptr, tabId);
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
		if (neu->embedContainer() && wid)
		{
			neu->embedContainer()->bind(tabId, wid);
		}
		neu->addTab(moved);
		if (moved.session)
		{
			moved.session->notifyReattachment(neu->shellId());
			moved.session->requestActivate(tabId);
		}
		if (neu->embedContainer() && wid)
		{
			neu->embedContainer()->resyncActive();
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
		if (neu->embedContainer())
		{
			neu->embedContainer()->resyncActive();
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
		const quintptr wid = EmbedContainer::transferBinding(source->embedContainer(), target->embedContainer(), tabId);
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
		if (target->embedContainer() && wid)
		{
			target->embedContainer()->resyncActive();
		}
		target->raise();
		target->activateWindow();
		// Never destroy the drag-source shell while QDrag / merge anim may still
		// reference it — defer until endTabDrag.
		if (m_dragActive || m_autoMergeAnimActive)
		{
			m_shellPendingDestroy = source;
		}
		else
		{
			destroyShellIfEmpty(source);
		}
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
		// Sole-Client whole-shell tear-out: the moving source sits under the cursor and
		// would always win hit-tests. Prefer other shells' strips so merge remains possible.
		const bool preferOtherShell = m_dragActive && m_dragMoveWholeShell && m_tearOutDetached;
		if (preferOtherShell)
		{
			// Exact strip hit first, then a wider magnetic band (Chrome-like merge aim).
			for (const auto& shell : m_shells)
			{
				if (!shell || !shell->isVisible() || shell.get() == m_dragSource)
				{
					continue;
				}
				if (shell->isOverTabDropZone(globalPos))
				{
					return shell.get();
				}
			}
			constexpr int kMergeMagnetV = tab_strip::kMergeMagnetV;
			constexpr int kMergeMagnetH = tab_strip::kMergeMagnetH;
			for (const auto& shell : m_shells)
			{
				if (!shell || !shell->isVisible() || shell.get() == m_dragSource)
				{
					continue;
				}
				if (shell->isNearTabDropZone(globalPos, kMergeMagnetV, kMergeMagnetH))
				{
					return shell.get();
				}
			}
			return nullptr;
		}
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
		// Whole-shell mode already moved the real window; "drop outside" means keep it.
		if (m_dragMoveWholeShell && m_tearOutDetached)
		{
			return false;
		}
		const bool overAny = tabDropZoneShellAtGlobal(globalPos) != nullptr;
		const bool nearLeave =
			m_dragSource && m_dragSource->isNearTabDropZone(globalPos, tab_strip::kTearOutLeaveSlopV, tab_strip::kTearOutLeaveSlopH);
		return tab_strip::shouldSuppressTearOut(overAny, nearLeave);
	}

	bool ShellApp::isReleaseOverWindowButtons(QPoint globalPos) const
	{
		for (const auto& s : m_shells)
		{
			if (s && s->isOverWindowButtons(globalPos))
			{
				return true;
			}
		}
		return false;
	}

	void ShellApp::flushCreatesDeferredDuringDrag()
	{
		if (m_deferredCreatesDuringDrag.empty())
		{
			return;
		}
		const auto pending = std::move(m_deferredCreatesDuringDrag);
		m_deferredCreatesDuringDrag.clear();
		for (const auto& item : pending)
		{
			if (!item.session || item.session->isDead())
			{
				continue;
			}
			if (item.preferredShell)
			{
				m_pendingFirstShell.insert(item.session, item.preferredShell);
			}
			item.session->requestCreateContentView(item.tabId, item.title);
		}
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
		m_dragAutoMerged = false;
		m_autoMergeAnimActive = false;
		m_pendingMergeTarget.clear();
		m_pendingMergeSource.clear();
		m_pendingMergeTabId = 0;
		m_pendingMergeIndex = -1;
		m_tearOutDetached = false;
		m_ghostSnapBackActive = false;
		m_dragForbiddenCursor = false;
		m_dragMoveWholeShell = tab_strip::shouldMoveWholeShellOnTearOut(source->clientTabCount());
		m_dragSource = source;
		m_dragTabId = tabId;
		m_dragResumeTabId = 0;
		m_dragPreviewSize = source->size();
		m_dragTabWidth = 0;
		m_dragSourceSavedGeometry = source->geometry();
		m_dragWindowHotSpot = QCursor::pos() - source->frameGeometry().topLeft();
		// Sole Client tab: follow immediately (no leave-slop dead zone). A second
		// drag-to-merge after release must move the shell from the first pixel.
		if (m_dragMoveWholeShell)
		{
			m_tearOutDetached = true;
			source->setWindowOpacity(0.92);
			// Accept drops on the full shell so OLE does not show Forbidden when the
			// cursor is over content/ribbon (strip-only targets are too narrow).
			for (auto& s : m_shells)
			{
				if (s)
				{
					s->setAcceptDrops(true);
				}
			}
			showDragDropSink(true);
		}
#ifdef Q_OS_WIN
		// Clear Esc transition bit so a prior Esc press is not mistaken for cancel.
		GetAsyncKeyState(VK_ESCAPE);
#endif

		for (const auto& t : source->tabs())
		{
			if (t.tabId == tabId)
			{
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
		if (source->embedContainer())
		{
			contentSnap = source->embedContainer()->grabContent(tabId, m_dragPreviewSize);
		}

		if (m_dragMoveWholeShell)
		{
			// Chrome last-tab: keep the Client tab active; the real shell is the feedback.
			source->setTabDragHidden(tabId, false);
			source->setActiveTab(tabId);
		}
		else
		{
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
		const int previewHx = qBound(16,
									 int(double(m_tabGhostHotSpot.x() - (m_tabDragGhost ? m_tabDragGhost->contentOrigin().x() : 0))
										 * double(m_dragPreviewSize.width()) / qMax(1, m_dragTabWidth)),
									 m_dragPreviewSize.width() - 16);
		m_dragHotSpot = QPoint(previewHx, TearOutPreview::kFramePad + TearOutPreview::kTitleBarHeight / 2);

		if (m_tearOutPreview)
		{
			if (m_dragMoveWholeShell)
			{
				m_tearOutPreview->hide();
				m_tearOutPreview->setContentPixmap({});
			}
			else
			{
				m_tearOutPreview->setContentPixmap(contentSnap);
				m_tearOutPreview->resize(m_dragPreviewSize);
				m_tearOutPreview->hide();
			}
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

	bool ShellApp::isDragAutoMerged() const
	{
		return m_dragAutoMerged;
	}

	bool ShellApp::isAutoMergeAnimating() const
	{
		return m_autoMergeAnimActive;
	}

	void ShellApp::pollEscapeCancel()
	{
#ifdef Q_OS_WIN
		// Windows OLE DoDragDrop often never delivers Qt KeyPress for Esc.
		if (m_dragAutoMerged || m_autoMergeAnimActive)
		{
			return;
		}
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

	void ShellApp::showDragDropSink(bool on)
	{
		if (!m_dragDropSink)
		{
			return;
		}
		if (!on)
		{
			m_dragDropSink->hide();
			return;
		}
		if (auto* sink = static_cast<TabDragDropSink*>(m_dragDropSink))
		{
			sink->follow(QCursor::pos());
		}
	}

	bool ShellApp::isDragDropSink(QObject* watched) const
	{
		return m_dragDropSink && watched == m_dragDropSink;
	}

	bool ShellApp::shellStillAlive(ShellWindow* shell) const
	{
		if (!shell)
		{
			return false;
		}
		for (const auto& s : m_shells)
		{
			if (s.get() == shell)
			{
				return true;
			}
		}
		return false;
	}

	void ShellApp::requestAbortOleDrag()
	{
#ifdef Q_OS_WIN
		// End DoDragDrop without a mouse release (magnetic auto-merge).
		// Scoped to our drag session: pollEscapeCancel ignores Esc while m_dragAutoMerged.
		if (!m_dragActive || !m_dragAutoMerged)
		{
			return;
		}
		INPUT in[2] = {};
		in[0].type = INPUT_KEYBOARD;
		in[0].ki.wVk = VK_ESCAPE;
		in[1].type = INPUT_KEYBOARD;
		in[1].ki.wVk = VK_ESCAPE;
		in[1].ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(2, in, sizeof(INPUT));
		// Clear transition bit so a later beginTabDrag does not see a stale Esc edge.
		GetAsyncKeyState(VK_ESCAPE);
#endif
	}

	void ShellApp::tryCommitMagneticAutoMerge()
	{
		// Spec + docs: magnetic auto-merge is for sole-Client whole-shell only.
		if (!m_dragMoveWholeShell)
		{
			return;
		}
		if (!m_dragActive || m_dragAutoMerged || m_dragDropHandled || m_dragCancelled || m_ghostSnapBackActive || m_autoMergeAnimActive)
		{
			return;
		}
		if (m_dragTabId == 0)
		{
			return;
		}
		const QPoint g = QCursor::pos();
		ShellWindow* otherShell = tabDropZoneShellAtGlobal(g);
		if (!otherShell || otherShell == m_dragSource || !otherShell->hasTabYieldPreview())
		{
			return;
		}
		// Do not auto-merge while aiming at chrome buttons (cancel zone).
		if (tab_strip::shouldCancelTearOutOverWindowButtons(isReleaseOverWindowButtons(g)))
		{
			return;
		}

		int insertIndex = otherShell->yieldInsertIndex();
		if (insertIndex < 0)
		{
			insertIndex = otherShell->tabInsertIndexAt(g);
		}

		const qint64 tabId = m_dragTabId;
		ShellWindow* source = m_dragSource;
		m_dragAutoMerged = true;
		noteTabDragDropHandled();
		showDragDropSink(false);
		// Keep target yield open during the settle animation.
		clearAllDropIndicators();
		for (auto& s : m_shells)
		{
			if (s && s.get() != otherShell)
			{
				s->clearTabYieldPreview();
			}
		}
		if (m_tearOutPreview)
		{
			m_tearOutPreview->hide();
		}

		startAutoMergeAnimation(source, otherShell, tabId, insertIndex);
		requestAbortOleDrag();
	}

	void ShellApp::startAutoMergeAnimation(ShellWindow* source, ShellWindow* target, qint64 tabId, int insertIndex)
	{
		m_pendingMergeTarget = target;
		m_pendingMergeSource = source;
		m_pendingMergeTabId = tabId;
		m_pendingMergeIndex = insertIndex;
		m_autoMergeAnimActive = true;
		m_finishAutoMergeGuard = false;

		if (m_autoMergeAnim)
		{
			QObject::disconnect(m_autoMergeAnim, nullptr, this, nullptr);
			m_autoMergeAnim->stop();
			m_autoMergeAnim->deleteLater();
			m_autoMergeAnim = nullptr;
		}

		constexpr int kMs = 200;
		const QRect slot = target ? target->tabDragSlotGlobalRect(tabId) : QRect();
		m_autoMergeAnim = new QParallelAnimationGroup(this);
		connect(m_autoMergeAnim, &QParallelAnimationGroup::finished, this, &ShellApp::finishAutoMergeAnimation);

		// Translate the real shell so its Client tab lands on the yield slot
		// (delta = slot - tab). Do not invent a shrink target — that flew to the
		// left of the strip. TearOutPreview flyers also mismatched screen coords.
		if (source && shellStillAlive(source) && m_dragMoveWholeShell)
		{
			if (m_tearOutPreview)
			{
				m_tearOutPreview->hide();
				m_tearOutPreview->setWindowOpacity(1.0);
				m_tearOutPreview->setContentPixmap({});
			}

			const QRect startGeo = source->geometry();
			QRect tabStart = source->tabDragSlotGlobalRect(tabId);
			if (!tabStart.isValid())
			{
				// Fallback when the source tab rect is briefly unavailable: approximate
				// Home width + strip margin (not a layout SSOT).
				constexpr int kApproxHomeAndMargin = 78;
				constexpr int kApproxTabTop = 6;
				const QSize ts = source->tabButtonSize(tabId);
				tabStart = QRect(source->mapToGlobal(QPoint(kApproxHomeAndMargin, kApproxTabTop)),
								 ts.isValid() ? ts : QSize(m_dragTabWidth > 0 ? m_dragTabWidth : 120, 28));
			}

			QRect endGeo = startGeo;
			if (slot.isValid())
			{
				endGeo = startGeo.translated(slot.topLeft() - tabStart.topLeft());
			}
			else if (target)
			{
				const QRect strip = target->tabStripGlobalRect();
				if (strip.isValid())
				{
					endGeo = startGeo.translated(strip.center() - tabStart.center());
				}
			}

			// Keep the real window visible while it slides; fade out as it settles.
			source->setWindowOpacity(qMax(0.85, source->windowOpacity()));
			source->raise();

			auto* winGeo = new QPropertyAnimation(source, "geometry", m_autoMergeAnim);
			winGeo->setDuration(kMs);
			winGeo->setEasingCurve(QEasingCurve::InOutCubic);
			winGeo->setStartValue(startGeo);
			winGeo->setEndValue(endGeo);
			m_autoMergeAnim->addAnimation(winGeo);

			auto* winOp = new QPropertyAnimation(source, "windowOpacity", m_autoMergeAnim);
			winOp->setDuration(kMs);
			winOp->setEasingCurve(QEasingCurve::InCubic);
			winOp->setStartValue(source->windowOpacity());
			winOp->setEndValue(0.0);
			m_autoMergeAnim->addAnimation(winOp);

			// Tab ghost rides the same delta so the face is readable over the fade.
			if (m_tabDragGhost && slot.isValid())
			{
				const QPixmap tabSnap = source->grabTabButton(tabId);
				const QSize tabSz = source->tabButtonSize(tabId);
				m_tabDragGhost->setTabPixmap(tabSnap.isNull() ? QPixmap() : tabSnap, tabSz.isValid() ? tabSz : tabStart.size());
				const QPoint origin = m_tabDragGhost->contentOrigin();
				const QRect tabGhostStart(tabStart.topLeft() - origin, m_tabDragGhost->size());
				const QRect tabGhostEnd(slot.topLeft() - origin, m_tabDragGhost->size());
				m_tabDragGhost->setGeometry(tabGhostStart);
				m_tabDragGhost->show();
				m_tabDragGhost->raise();

				auto* tabGeo = new QPropertyAnimation(m_tabDragGhost, "geometry", m_autoMergeAnim);
				tabGeo->setDuration(kMs);
				tabGeo->setEasingCurve(QEasingCurve::InOutCubic);
				tabGeo->setStartValue(tabGhostStart);
				tabGeo->setEndValue(tabGhostEnd);
				m_autoMergeAnim->addAnimation(tabGeo);
			}
			else if (m_tabDragGhost)
			{
				m_tabDragGhost->hide();
			}

			m_autoMergeAnim->start();
			return;
		}

		if (m_tabDragGhost && slot.isValid())
		{
			if (m_tearOutPreview)
			{
				m_tearOutPreview->hide();
			}
			const QPoint origin = m_tabDragGhost->contentOrigin();
			const QRect end(slot.topLeft() - origin, m_tabDragGhost->size());
			const QRect startGeo = m_tabDragGhost->isVisible() ? m_tabDragGhost->geometry()
															   : QRect(QCursor::pos() - origin - QPoint(40, 10), m_tabDragGhost->size());
			if (!m_tabDragGhost->isVisible())
			{
				m_tabDragGhost->setGeometry(startGeo);
				m_tabDragGhost->show();
			}
			m_tabDragGhost->raise();

			auto* geo = new QPropertyAnimation(m_tabDragGhost, "geometry", m_autoMergeAnim);
			geo->setDuration(kMs);
			geo->setEasingCurve(QEasingCurve::InOutCubic);
			geo->setStartValue(m_tabDragGhost->geometry());
			geo->setEndValue(end);
			m_autoMergeAnim->addAnimation(geo);
			m_autoMergeAnim->start();
			return;
		}

		finishAutoMergeAnimation();
	}

	void ShellApp::finishAutoMergeAnimation()
	{
		if (m_finishAutoMergeGuard)
		{
			return;
		}
		if (!m_autoMergeAnimActive && m_pendingMergeTabId == 0)
		{
			return;
		}
		m_finishAutoMergeGuard = true;
		m_autoMergeAnimActive = false;
		if (m_autoMergeAnim)
		{
			QObject::disconnect(m_autoMergeAnim, nullptr, this, nullptr);
			m_autoMergeAnim->deleteLater();
			m_autoMergeAnim = nullptr;
		}

		const qint64 tabId = m_pendingMergeTabId;
		ShellWindow* target = m_pendingMergeTarget.data();
		const int insertIndex = m_pendingMergeIndex;
		ShellWindow* source = m_pendingMergeSource.data();
		if (!source)
		{
			source = m_dragSource;
		}
		m_pendingMergeTabId = 0;
		m_pendingMergeTarget.clear();
		m_pendingMergeSource.clear();
		m_pendingMergeIndex = -1;

		if (m_tabDragGhost)
		{
			m_tabDragGhost->hide();
		}
		if (m_tearOutPreview)
		{
			m_tearOutPreview->hide();
			m_tearOutPreview->setWindowOpacity(1.0);
			m_tearOutPreview->setContentPixmap({});
		}
		clearAllDropIndicators();
		clearAllTabYieldPreviews();

		if (source && shellStillAlive(source))
		{
			source->setWindowOpacity(1.0);
		}

		if (tabId != 0 && target && shellStillAlive(target))
		{
			mergeTab(tabId, target, insertIndex);
		}

		if (source && !shellStillAlive(source))
		{
			m_dragSource = nullptr;
		}

		if (m_dragActive)
		{
			endTabDrag(/*tearOrMerge=*/false);
		}
		m_finishAutoMergeGuard = false;
	}

	void ShellApp::startGhostSnapBack()
	{
		if (!m_dragSource || m_dragTabId == 0)
		{
			finishGhostSnapBack();
			return;
		}
		if (m_tearOutPreview)
		{
			m_tearOutPreview->hide();
		}
		m_tearOutDetached = false;

		if (m_dragMoveWholeShell)
		{
			// Snap the real shell back to its pre-drag geometry (no TearOutPreview).
			if (m_tabDragGhost)
			{
				m_tabDragGhost->hide();
			}
			m_dragSource->setWindowOpacity(1.0);
			const QRect start = m_dragSource->geometry();
			const QRect end = m_dragSourceSavedGeometry.isValid() ? m_dragSourceSavedGeometry : start;
			m_ghostSnapBackActive = true;
			if (!m_ghostSnapAnim)
			{
				m_ghostSnapAnim = new QPropertyAnimation(this);
				m_ghostSnapAnim->setDuration(160);
				m_ghostSnapAnim->setEasingCurve(QEasingCurve::InOutCubic);
				connect(m_ghostSnapAnim, &QPropertyAnimation::finished, this, &ShellApp::finishGhostSnapBack);
			}
			m_ghostSnapAnim->stop();
			m_ghostSnapAnim->setTargetObject(m_dragSource);
			m_ghostSnapAnim->setPropertyName("geometry");
			m_ghostSnapAnim->setStartValue(start);
			m_ghostSnapAnim->setEndValue(end);
			m_ghostSnapAnim->start();
			return;
		}

		if (!m_tabDragGhost)
		{
			finishGhostSnapBack();
			return;
		}

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
		m_ghostSnapAnim->setTargetObject(m_tabDragGhost);
		m_ghostSnapAnim->setPropertyName("geometry");
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
		if (m_dragSource)
		{
			m_dragSource->setWindowOpacity(1.0);
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
		if (m_dragMoveWholeShell && m_dragSource)
		{
			return m_dragSource->geometry();
		}
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
		if (m_autoMergeAnim)
		{
			QObject::disconnect(m_autoMergeAnim, nullptr, this, nullptr);
			m_autoMergeAnim->stop();
			m_autoMergeAnim->deleteLater();
			m_autoMergeAnim = nullptr;
		}
		m_autoMergeAnimActive = false;
		m_finishAutoMergeGuard = false;
		m_pendingMergeTarget.clear();
		m_pendingMergeSource.clear();
		m_pendingMergeTabId = 0;
		m_pendingMergeIndex = -1;
		m_ghostSnapBackActive = false;
		m_tearOutDetached = false;
		const bool moveWholeShell = m_dragMoveWholeShell;
		const QRect savedGeo = m_dragSourceSavedGeometry;
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
		m_dragMoveWholeShell = false;
		m_dragSourceSavedGeometry = {};
		m_dragWindowHotSpot = {};
		m_dragAutoMerged = false;
		showDragDropSink(false);

		if (moveWholeShell)
		{
			for (auto& s : m_shells)
			{
				if (s)
				{
					s->setAcceptDrops(false);
				}
			}
		}

		if (!source)
		{
			flushCreatesDeferredDuringDrag();
			return;
		}

		source->setWindowOpacity(1.0);

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
			else if (!dropHandled && moveWholeShell)
			{
				source->setActiveTab(tabId);
				if (savedGeo.isValid())
				{
					source->setGeometry(savedGeo);
				}
			}
		}
		else if (stillHere && tearOrMerge && moveWholeShell)
		{
			// Drop outside: keep window where it was dragged; unhide/activate tab.
			source->setTabDragHidden(tabId, false);
			source->setActiveTab(tabId);
		}
		clearAllDropIndicators();
		// Spec S5: emit deferred CreateSubWindow after drag ends (and suppress is cleared).
		flushCreatesDeferredDuringDrag();

		if (m_shellPendingDestroy)
		{
			ShellWindow* doomed = m_shellPendingDestroy.data();
			m_shellPendingDestroy.clear();
			if (doomed && shellStillAlive(doomed))
			{
				destroyShellIfEmpty(doomed);
			}
		}
	}

	void ShellApp::updateTabDragVisuals()
	{
		if (!m_dragActive || m_dragAutoMerged)
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
		if (m_dragMoveWholeShell)
		{
			if (auto* sink = static_cast<TabDragDropSink*>(m_dragDropSink))
			{
				sink->follow(g);
			}
		}

		// Forbidden cursor over window min/max/close (not a drop target).
		// Whole-shell drag accepts Move everywhere under the moving window; never
		// flip to Forbidden (OLE + overlapping chrome would flash the red circle).
		const bool forbidden = !m_dragMoveWholeShell && isReleaseOverWindowButtons(g);
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
			m_dragSource && m_dragSource->isNearTabDropZone(g, tab_strip::kTearOutLeaveSlopV, tab_strip::kTearOutLeaveSlopH);
		const bool nearReturn =
			m_dragSource && m_dragSource->isNearTabDropZone(g, tab_strip::kTearOutReturnSlopV, tab_strip::kTearOutReturnSlopH);

		const bool wasDetached = m_tearOutDetached;
		if (m_dragMoveWholeShell)
		{
			// Stay detached for the whole drag (merge is other-shell strip hover/drop).
			m_tearOutDetached = true;
		}
		else
		{
			m_tearOutDetached = tab_strip::nextTearOutDetached(wasDetached, overStrip, nearLeave, nearReturn);
		}

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

		if (m_dragMoveWholeShell)
		{
			if (m_tearOutPreview)
			{
				m_tearOutPreview->hide();
			}
			if (!m_tearOutDetached)
			{
				if (wasDetached && m_dragSource)
				{
					m_dragSource->setWindowOpacity(1.0);
					if (m_dragTabId != 0)
					{
						m_dragSource->setTabDragHidden(m_dragTabId, false);
					}
				}
				ShellWindow* stripShell = tabDropZoneShellAtGlobal(g);
				if (!stripShell)
				{
					stripShell = m_dragSource;
				}
				// Sole Client tab stays visible on the real shell — do not run source
				// yield (that hides the only Client tab and leaves Home alone).
				if (m_dragSource)
				{
					m_dragSource->clearTabYieldPreview();
					if (m_dragTabId != 0)
					{
						m_dragSource->setTabDragHidden(m_dragTabId, false);
					}
				}
				for (auto& s : m_shells)
				{
					if (s && s.get() != m_dragSource)
					{
						s->clearDropInsertIndicator();
						s->clearTabYieldPreview();
					}
				}
				if (m_tabDragGhost)
				{
					m_tabDragGhost->hide();
				}
				return;
			}

			// Detached: move the real shell (Chrome last-tab). Capture hotspot once
			// so the window does not jump under the cursor.
			if (!wasDetached && m_dragSource)
			{
				clearAllDropIndicators();
				m_dragSource->clearTabYieldPreview();
				if (m_dragTabId != 0)
				{
					m_dragSource->setTabDragHidden(m_dragTabId, false);
				}
				m_dragWindowHotSpot = g - m_dragSource->frameGeometry().topLeft();
				m_dragSource->setWindowOpacity(0.92);
			}
			if (m_tabDragGhost)
			{
				m_tabDragGhost->hide();
			}
			if (m_dragSource)
			{
				if (m_dragTabId != 0)
				{
					m_dragSource->setTabDragHidden(m_dragTabId, false);
				}
				const QPoint topLeft = g - m_dragWindowHotSpot;
				if (m_dragSource->frameGeometry().topLeft() != topLeft)
				{
					m_dragSource->move(topLeft);
				}
				m_dragSource->raise();
			}
			ShellWindow* otherShell = tabDropZoneShellAtGlobal(g);
			const int guestW = m_dragTabWidth > 0 ? m_dragTabWidth : 80;
			if (otherShell && otherShell != m_dragSource && m_dragTabId != 0)
			{
				for (auto& s : m_shells)
				{
					if (s && s.get() != otherShell)
					{
						s->clearTabYieldPreview();
						s->clearDropInsertIndicator();
					}
				}
				otherShell->clearDropInsertIndicator();
				otherShell->previewTabYieldAtCursor(m_dragTabId, g, guestW, contentHotX);
				tryCommitMagneticAutoMerge();
			}
			else
			{
				for (auto& s : m_shells)
				{
					if (s && s.get() != m_dragSource)
					{
						s->clearTabYieldPreview();
						s->clearDropInsertIndicator();
					}
				}
			}
			return;
		}

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
				const int guestW = m_dragTabWidth > 0 ? m_dragTabWidth : (m_tabDragGhost ? m_tabDragGhost->contentSize().width() : 80);
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

	QString ShellApp::makeTitle(int instanceIndex, int contentIndex) const
	{
		return QStringLiteral("Client%1-Tab%2").arg(instanceIndex).arg(contentIndex);
	}
} // namespace mps::host
