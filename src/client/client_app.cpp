#include "client_app.hpp"

#include "envelope_builder.hpp"
#include "heartbeat_policy.hpp"
#include "qfluentribbon/qfluentribbon.hpp"
#include "qtheme/engine.hpp"
#include "qtheme/types.hpp"
#include "theme_scheme.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::client
{
	namespace
	{
		QAction* makeAction(QWidget* parent, const QString& id, const QString& text, QStyle::StandardPixmap icon, const QString& tipBody)
		{
			auto* action = new QAction(text, parent);
			action->setObjectName(id);
			action->setIcon(parent->style()->standardIcon(icon));
			qfluentribbon::ScreenTip::set(action, text, tipBody);
			return action;
		}
	} // namespace

	PageWindow::PageWindow(qint64 tabId, QString title, qfluentribbon::ThemeBridge* bridge, qtheme::Engine* engine, QWidget* parent)
		: qfluentribbon::RibbonWindow(parent)
		, m_tabId(tabId)
		, m_pendingBridge(bridge)
		, m_pendingEngine(engine)
	{
		// Match Demo embed contract: frameless top-level + native HWND before SubWindowAdded.
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setAttribute(Qt::WA_DeleteOnClose, false);
		setAttribute(Qt::WA_NativeWindow);
		setWindowTitle(title);
		setMinimumSize(0, 0);
		resize(640, 480);
		// Ribbon/icons are built in realizeChrome() after HWND+screen are bound.
	}

	void PageWindow::realizeChrome()
	{
		if (m_chromeReady || !m_pendingBridge || !m_pendingEngine)
		{
			return;
		}
		setThemeBridge(m_pendingBridge);
		buildRibbon(m_pendingBridge, m_pendingEngine);
		m_pendingBridge = nullptr;
		m_pendingEngine = nullptr;
		m_chromeReady = true;
	}

	bool PageWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
	{
#ifdef Q_OS_WIN
		if (eventType == QByteArrayLiteral("windows_generic_MSG") || eventType == QByteArrayLiteral("windows_dispatcher_MSG"))
		{
			const auto* msg = static_cast<const MSG*>(message);
			if (msg && (msg->message == WM_WINDOWPOSCHANGED || msg->message == WM_SIZE))
			{
				if (!m_embedSyncPending)
				{
					m_embedSyncPending = true;
					QTimer::singleShot(0, this,
									   [this]
									   {
										   m_embedSyncPending = false;
										   syncAfterEmbed();
									   });
				}
			}
		}
#else
		Q_UNUSED(eventType);
		Q_UNUSED(message);
		Q_UNUSED(result);
#endif
		return qfluentribbon::RibbonWindow::nativeEvent(eventType, message, result);
	}

	void PageWindow::syncAfterEmbed()
	{
#ifdef Q_OS_WIN
		const HWND hwnd = reinterpret_cast<HWND>(winId());
		if (!hwnd || !IsWindow(hwnd) || !GetParent(hwnd))
		{
			return;
		}

		RECT rc{};
		GetClientRect(hwnd, &rc);
		const int physW = qMax(1, static_cast<int>(rc.right - rc.left));
		const int physH = qMax(1, static_cast<int>(rc.bottom - rc.top));
		const qreal dpr = qMax(qreal(1), devicePixelRatioF());
		const int logicalW = qMax(1, qRound(static_cast<qreal>(physW) / dpr));
		const int logicalH = qMax(1, qRound(static_cast<qreal>(physH) / dpr));
		const QSize target(logicalW, logicalH);
		const bool sizeChanged = size() != target;
		if (sizeChanged)
		{
			resize(target);
		}

		// First settled embed (or any later size jump): nudge backing store so Qt does not
		// keep a stretched birth-size buffer inside the larger HWND.
		if (!m_embedSynced || sizeChanged)
		{
			m_embedSynced = true;
			resize(target + QSize(1, 0));
			resize(target);
			if (ribbonBar())
			{
				ribbonBar()->polishFromStore();
			}
			if (testAttribute(Qt::WA_DontShowOnScreen))
			{
				setAttribute(Qt::WA_DontShowOnScreen, false);
				show();
			}
			repaint();
		}
#endif
	}

	void PageWindow::buildRibbon(qfluentribbon::ThemeBridge* bridge, qtheme::Engine* engine)
	{
		auto* status = new QLabel(windowTitle(), this);
		status->setAlignment(Qt::AlignCenter);
		QFont f = status->font();
		f.setPointSize(16);
		f.setBold(true);
		status->setFont(f);
		const QString pageTitle = windowTitle();

		auto* newWindowBtn = new QPushButton(QStringLiteral("新建窗口"), this);
		newWindowBtn->setFixedSize(140, 36);
		connect(newWindowBtn, &QPushButton::clicked, this, &PageWindow::requestNewWindow);

		auto* central = new QWidget(this);
		auto* lay = new QVBoxLayout(central);
		lay->setContentsMargins(16, 16, 16, 16);

		auto* pinRow = new QHBoxLayout();
		pinRow->addWidget(new QLabel(QStringLiteral("Pin to QAT:"), central));
		auto* pinCopy = new QPushButton(QStringLiteral("Copy"), central);
		auto* pinGrid = new QPushButton(QStringLiteral("Grid"), central);
		auto* clearQat = new QPushButton(QStringLiteral("Clear QAT"), central);
		pinRow->addWidget(pinCopy);
		pinRow->addWidget(pinGrid);
		pinRow->addWidget(clearQat);
		pinRow->addStretch(1);
		lay->addLayout(pinRow);

		lay->addStretch();
		lay->addWidget(status, 0, Qt::AlignCenter);
		lay->addSpacing(12);
		lay->addWidget(newWindowBtn, 0, Qt::AlignCenter);
		lay->addStretch();
		setCentralWidget(central);

		auto* ribbon = ribbonBar();
		auto* home = ribbon->addTab(QStringLiteral("Home"));
		auto* insert = ribbon->addTab(QStringLiteral("Insert"));
		auto* view = ribbon->addTab(QStringLiteral("View"));
		if (QTabBar* tabs = ribbon->tabBar())
		{
			tabs->setTabData(0, QStringLiteral("H"));
			tabs->setTabData(1, QStringLiteral("N"));
			tabs->setTabData(2, QStringLiteral("W"));
		}

		auto* paste = makeAction(this, QStringLiteral("clipboard.paste"), QStringLiteral("Paste"), QStyle::SP_DialogOpenButton,
								 QStringLiteral("Paste clipboard contents."));
		auto* cut = makeAction(this, QStringLiteral("clipboard.cut"), QStringLiteral("Cut"), QStyle::SP_DialogResetButton,
							   QStringLiteral("Cut the selection."));
		auto* copy = makeAction(this, QStringLiteral("clipboard.copy"), QStringLiteral("Copy"), QStyle::SP_FileDialogDetailedView,
								QStringLiteral("Copy the selection."));
		auto* bold = makeAction(this, QStringLiteral("font.bold"), QStringLiteral("Bold"), QStyle::SP_DialogApplyButton,
								QStringLiteral("Make text bold."));
		auto* italic = makeAction(this, QStringLiteral("font.italic"), QStringLiteral("Italic"), QStyle::SP_DialogYesButton,
								  QStringLiteral("Italicize text."));
		auto* underline = makeAction(this, QStringLiteral("font.underline"), QStringLiteral("Underline"), QStyle::SP_ArrowDown,
									 QStringLiteral("Underline the selection."));
		auto* bullets = makeAction(this, QStringLiteral("para.bullets"), QStringLiteral("Bullets"), QStyle::SP_BrowserReload,
								   QStringLiteral("Start a bulleted list."));
		auto* align = makeAction(this, QStringLiteral("para.align"), QStringLiteral("Align"), QStyle::SP_ArrowLeft,
								 QStringLiteral("Change paragraph alignment."));
		auto* table = makeAction(this, QStringLiteral("insert.table"), QStringLiteral("Table"), QStyle::SP_FileDialogListView,
								 QStringLiteral("Insert a table."));
		auto* chart = makeAction(this, QStringLiteral("insert.chart"), QStringLiteral("Chart"), QStyle::SP_FileDialogContentsView,
								 QStringLiteral("Insert a chart."));
		auto* grid = makeAction(this, QStringLiteral("view.grid"), QStringLiteral("Grid"), QStyle::SP_ComputerIcon,
								QStringLiteral("Toggle the grid."));
		auto* ruler = makeAction(this, QStringLiteral("view.ruler"), QStringLiteral("Ruler"), QStyle::SP_DesktopIcon,
								 QStringLiteral("Toggle the ruler."));
		auto* newWindow = makeAction(this, QStringLiteral("window.new"), QStringLiteral("New Window"), QStyle::SP_FileDialogNewFolder,
									 QStringLiteral("Request another embedded page."));
		auto* light = makeAction(this, QStringLiteral("theme.light"), QStringLiteral("Light"), QStyle::SP_DialogApplyButton,
								 QStringLiteral("Fluent Light skin."));
		auto* dark = makeAction(this, QStringLiteral("theme.dark"), QStringLiteral("Dark"), QStyle::SP_ComputerIcon,
								QStringLiteral("Fluent Dark skin."));

		auto* clipboard = home->addGroup(QStringLiteral("Clipboard"));
		(void)clipboard->addAction(paste);
		(void)clipboard->addAction(cut);
		(void)clipboard->addAction(copy);

		auto* fontGroup = home->addGroup(QStringLiteral("Font"));
		(void)fontGroup->addAction(bold);
		(void)fontGroup->addAction(italic);
		(void)fontGroup->addAction(underline);

		auto* para = home->addGroup(QStringLiteral("Paragraph"));
		(void)para->addAction(bullets);
		(void)para->addAction(align);

		auto* windowGroup = home->addGroup(QStringLiteral("Window"));
		(void)windowGroup->addAction(newWindow);
		connect(newWindow, &QAction::triggered, this, &PageWindow::requestNewWindow);

		auto* themeGroup = home->addGroup(QStringLiteral("Theme"));
		(void)themeGroup->addAction(light);
		(void)themeGroup->addAction(dark);

		auto* tables = insert->addGroup(QStringLiteral("Tables"));
		(void)tables->addAction(table);
		(void)tables->addAction(chart);

		auto* show = view->addGroup(QStringLiteral("Show"));
		(void)show->addAction(grid);
		(void)show->addAction(ruler);

		qfluentribbon::QuickAccessBar* qat = ribbon->quickAccessBar();
		QHash<QString, QAction*> catalog;
		for (QAction* action : {paste, cut, copy, bold, italic, underline, bullets, align, table, chart, grid, ruler, newWindow})
		{
			catalog.insert(action->objectName(), action);
		}

		if (qat)
		{
			QSettings settings(QStringLiteral("yanxijian"), QStringLiteral("mps_demo_client"));
			connect(qat, &qfluentribbon::QuickAccessBar::actionsChanged, this,
					[qat, status]()
					{
						QSettings s(QStringLiteral("yanxijian"), QStringLiteral("mps_demo_client"));
						qat->saveState(s);
						status->setText(QStringLiteral("QAT updated (%1 pinned)").arg(qat->actions().size()));
					});

			const int restored = qat->restoreState(settings, catalog);
			if (restored == 0)
			{
				(void)qat->addAction(paste);
				(void)qat->addAction(bold);
				(void)qat->addAction(newWindow);
			}

			connect(pinCopy, &QPushButton::clicked, this,
					[qat, copy, status]()
					{
						if (qat->addAction(copy))
						{
							status->setText(QStringLiteral("Pinned Copy to QAT"));
						}
						else
						{
							status->setText(QStringLiteral("Copy already on QAT"));
						}
					});
			connect(pinGrid, &QPushButton::clicked, this,
					[qat, grid, status]()
					{
						if (qat->addAction(grid))
						{
							status->setText(QStringLiteral("Pinned Grid to QAT"));
						}
						else
						{
							status->setText(QStringLiteral("Grid already on QAT"));
						}
					});
			connect(clearQat, &QPushButton::clicked, qat, &qfluentribbon::QuickAccessBar::clear);
		}

		auto wireStatus = [status, pageTitle](QAction* action)
		{
			connect(action, &QAction::triggered, status,
					[status, pageTitle, action]()
					{
						status->setText(QStringLiteral("%1 — %2").arg(pageTitle, action->text()));
					});
		};
		for (QAction* action : {paste, cut, copy, bold, italic, underline, bullets, align, table, chart, grid, ruler, newWindow})
		{
			wireStatus(action);
		}

		if (engine)
		{
			connect(light, &QAction::triggered, this,
					[this, status]()
					{
						emit requestThemeScheme(qtheme::ColorScheme::Light);
						status->setText(QStringLiteral("Requesting Fluent Light…"));
					});
			connect(dark, &QAction::triggered, this,
					[this, status]()
					{
						emit requestThemeScheme(qtheme::ColorScheme::Dark);
						status->setText(QStringLiteral("Requesting Fluent Dark…"));
					});
		}

		Q_UNUSED(bridge);
		Q_UNUSED(engine);
		qfluentribbon::ScreenTip::install(this);
	}

	ClientApp::ClientApp(QString endpoint, QString token, bool enableHeartbeat, QObject* parent)
		: QObject(parent)
		, m_endpoint(std::move(endpoint))
		, m_token(std::move(token))
		, m_enableHeartbeat(enableHeartbeat)
	{
	}

	ClientApp::~ClientApp() = default;

	void ClientApp::ensureTheme()
	{
		if (m_engine)
		{
			return;
		}
		auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
		if (!app)
		{
			return;
		}
		m_engine = std::make_unique<qtheme::Engine>();
		m_engine->apply(app);
		m_bridge = std::make_unique<qfluentribbon::ThemeBridge>();
		m_bridge->bind(m_engine.get());
	}

	void ClientApp::applyThemeScheme(qtheme::ColorScheme scheme)
	{
		ensureTheme();
		if (!m_engine)
		{
			return;
		}
		if (scheme != qtheme::ColorScheme::Light && scheme != qtheme::ColorScheme::Dark)
		{
			scheme = qtheme::ColorScheme::Light;
		}
		(void)m_engine->setColorScheme(scheme, /*force=*/true);
		for (auto it = m_pages.begin(); it != m_pages.end(); ++it)
		{
			if (PageWindow* page = it.value())
			{
				if (page->ribbonBar())
				{
					page->ribbonBar()->polishFromStore();
				}
			}
		}
	}

	void ClientApp::requestThemeFromHost(qtheme::ColorScheme scheme, qint64 tabId)
	{
		const mps::theme::Scheme wire = (scheme == qtheme::ColorScheme::Dark) ? mps::theme::Scheme::Dark : mps::theme::Scheme::Light;
		// Optimistic local apply; Host broadcast / InvokeResult remains the SSOT.
		applyThemeScheme(wire == mps::theme::Scheme::Dark ? qtheme::ColorScheme::Dark : qtheme::ColorScheme::Light);
		if (!m_channel)
		{
			return;
		}
		const QByteArray params = mps::theme::toParams(wire);
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		env.mutable_invoke()->set_method("theme.set");
		env.mutable_invoke()->set_params(params.constData(), static_cast<int>(params.size()));
		m_channel->send(env);
	}

	bool ClientApp::connectToHost()
	{
		ensureTheme();
		m_socket = new QLocalSocket(this);
		m_socket->connectToServer(m_endpoint);
		if (!m_socket->waitForConnected(5000))
		{
			qWarning("connect failed: %s", qPrintable(m_socket->errorString()));
			return false;
		}
		m_channel = std::make_unique<mps::ipc::EnvelopeChannel>(m_socket, this);
		m_channel->setHandler(
			[this](shell::ipc::v1::Envelope env)
			{
				onEnvelope(std::move(env));
			});
		connect(m_channel.get(), &mps::ipc::EnvelopeChannel::disconnected, this,
				[this]
				{
					stopHeartbeatTimer();
					qApp->quit();
				});
		sendHello();
		return true;
	}

	void ClientApp::startHeartbeatTimer()
	{
		if (!m_enableHeartbeat || m_heartbeatArmed || !m_channel)
		{
			return;
		}
		m_heartbeatArmed = true;
		if (!m_heartbeatTimer)
		{
			m_heartbeatTimer = new QTimer(this);
			m_heartbeatTimer->setInterval(static_cast<int>(mps::ipc::kHeartbeatIntervalMs));
			connect(m_heartbeatTimer, &QTimer::timeout, this, &ClientApp::sendHeartbeat);
		}
		sendHeartbeat();
		m_heartbeatTimer->start();
	}

	void ClientApp::stopHeartbeatTimer()
	{
		if (m_heartbeatTimer)
		{
			m_heartbeatTimer->stop();
		}
		m_heartbeatArmed = false;
	}

	void ClientApp::sendHeartbeat()
	{
		if (!m_channel)
		{
			return;
		}
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		env.mutable_heartbeat();
		m_channel->send(env);
	}

	void ClientApp::sendHello()
	{
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		auto* hello = env.mutable_hello();
		hello->set_min_protocol(1);
		hello->set_max_protocol(1);
#ifdef Q_OS_WIN
		hello->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		hello->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		hello->set_app_name("demo_client");
		auto* caps = hello->mutable_caps();
		caps->set_embed(shell::ipc::v1::EMBED_HWND);
		caps->set_tab_drag(true);
		caps->set_heartbeat(m_enableHeartbeat);
		caps->set_invoke(true);
		caps->set_multi_sub_window(true);
		m_channel->send(env);
	}

	void ClientApp::ensureMainReported()
	{
		if (m_mainReported || m_pages.isEmpty())
		{
			return;
		}
		auto* first = m_pages.begin().value();
		first->winId();
		auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch());
		auto* added = env.mutable_main_window_added();
		added->set_wid(static_cast<uint64_t>(first->winId()));
#ifdef Q_OS_WIN
		added->set_pid(static_cast<uint32_t>(GetCurrentProcessId()));
#else
		added->set_pid(static_cast<uint32_t>(QCoreApplication::applicationPid()));
#endif
		added->set_visible(true);
		m_channel->send(env);
		m_mainReported = true;
	}

	void ClientApp::createPage(qint64 tabId, const QString& title)
	{
		ensureTheme();
#ifdef Q_OS_WIN
		// Sibling pages may be created after an earlier HWND was SetParent'd into Host;
		// re-assert PMV2 so the new top-level HWND is not born under a degraded thread context.
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
		auto* page = new PageWindow(tabId, title, m_bridge.get(), m_engine.get());
		m_pages.insert(tabId, page);
		connect(page, &PageWindow::requestNewWindow, this,
				[this, tabId]
				{
					auto env = mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_REQ,
													  QDateTime::currentMSecsSinceEpoch(), 0, tabId);
					env.mutable_invoke()->set_method("demo.request_new_window");
					m_channel->send(env);
				});
		connect(page, &PageWindow::requestThemeScheme, this,
				[this, tabId](qtheme::ColorScheme scheme)
				{
					requestThemeFromHost(scheme, tabId);
				});
		page->createWinId();
		if (QWindow* wh = page->windowHandle())
		{
			if (QScreen* screen = QGuiApplication::primaryScreen())
			{
				wh->setScreen(screen);
			}
		}
		page->realizeChrome();
#ifdef Q_OS_WIN
		// Defer first on-screen paint until Host SetParent + SetWindowPos (see syncAfterEmbed).
		page->setAttribute(Qt::WA_DontShowOnScreen, true);
#endif
		page->show();
		page->winId();
		if (page->ribbonBar())
		{
			page->ribbonBar()->polishFromStore();
		}

		if (!m_mainReported)
		{
			ensureMainReported();
		}

		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		auto* added = env.mutable_sub_window_added();
		added->set_title(title.toStdString());
		added->set_wid(static_cast<uint64_t>(page->winId()));
		m_channel->send(env);

		activatePage(tabId);
#ifdef Q_OS_WIN
		// Fallback if WM_SIZE was missed; syncAfterEmbed no-ops until GetParent is set.
		QTimer::singleShot(50, page, &PageWindow::syncAfterEmbed);
#endif
	}

	void ClientApp::closePage(qint64 tabId)
	{
		auto* page = m_pages.take(tabId);
		if (!page)
		{
			return;
		}
		if (m_active == page)
		{
			m_active = nullptr;
		}
		page->hide();
		page->deleteLater();
		auto env =
			mps::ipc::makeEnvelope(1, mps::ipc::newCorrelationId(), shell::ipc::v1::DIR_EVT, QDateTime::currentMSecsSinceEpoch(), 0, tabId);
		env.mutable_sub_window_removed();
		m_channel->send(env);
	}

	void ClientApp::activatePage(qint64 tabId)
	{
		auto* page = m_pages.value(tabId, nullptr);
		if (!page)
		{
			return;
		}
		// Do NOT hide other pages: each may be SetParent'd into a different Host shell.
		// Visibility of non-active embeds is owned by the Host (ShowWindow / clearForeignWindow).
		page->show();
		m_active = page;
	}

	void ClientApp::onEnvelope(shell::ipc::v1::Envelope env)
	{
		if (env.has_hello_ack())
		{
			if (m_enableHeartbeat && env.hello_ack().host_caps().heartbeat())
			{
				startHeartbeatTimer();
			}
			return;
		}
		if (env.has_create_sub_window())
		{
			createPage(env.tab_id(), QString::fromStdString(env.create_sub_window().title()));
			return;
		}
		if (env.has_active_sub_window())
		{
			activatePage(env.tab_id());
			return;
		}
		if (env.has_query_close_sub_window())
		{
			auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch(), env.tab_id());
			res.mutable_query_close_sub_window_result()->set_accept(true);
			m_channel->send(res);
			closePage(env.tab_id());
			return;
		}
		if (env.has_set_drag_suppress() || env.has_notify_main_window_reattachment())
		{
			// Demo Client intentionally ignores these EVTs (Host still emits for contract).
			// See docs/zh/m5-gap-audit.md G2/G3 — accepted Demo limit.
			return;
		}
		if (env.has_invoke())
		{
			if (env.invoke().method() == "theme.set")
			{
				const QByteArray params = QByteArray::fromStdString(env.invoke().params());
				mps::theme::Scheme wire = mps::theme::Scheme::Light;
				if (!mps::theme::fromParams(params, &wire))
				{
					auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch());
					auto* err = res.mutable_error();
					err->set_code(shell::ipc::v1::ERROR_PROTOCOL);
					err->set_message("theme.set params must be light or dark");
					m_channel->send(res);
					return;
				}
				applyThemeScheme(wire == mps::theme::Scheme::Dark ? qtheme::ColorScheme::Dark : qtheme::ColorScheme::Light);
				const QByteArray wireBytes = mps::theme::toParams(wire);
				auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch());
				res.mutable_invoke_result()->set_payload(wireBytes.constData(), static_cast<int>(wireBytes.size()));
				m_channel->send(res);
				return;
			}
			auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch());
			auto* err = res.mutable_error();
			err->set_code(shell::ipc::v1::ERROR_UNIMPLEMENTED);
			err->set_message("unimplemented");
			m_channel->send(res);
			return;
		}
		if (env.has_ping())
		{
			auto res = mps::ipc::makeResponse(1, env.id(), QDateTime::currentMSecsSinceEpoch());
			res.mutable_pong();
			m_channel->send(res);
		}
	}
} // namespace mps::client
