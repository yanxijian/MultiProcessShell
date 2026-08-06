#include "ribbon_content_window.hpp"

#include "qfluentribbon/qfluentribbon.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::demo
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

	RibbonContentWindow::RibbonContentWindow(qint64 tabId, QString title, qfluentribbon::ThemeBridge* bridge, QWidget* parent)
		: qfluentribbon::RibbonWindow(parent)
		, m_tabId(tabId)
		, m_pendingBridge(bridge)
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

	void RibbonContentWindow::realizeChrome()
	{
		if (m_chromeReady || !m_pendingBridge)
		{
			return;
		}
		setThemeBridge(m_pendingBridge);
		buildRibbon(m_pendingBridge);
		m_pendingBridge = nullptr;
		m_chromeReady = true;
	}

	bool RibbonContentWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
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

	void RibbonContentWindow::syncAfterEmbed()
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

	void RibbonContentWindow::buildRibbon(qfluentribbon::ThemeBridge* bridge)
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
		connect(newWindowBtn, &QPushButton::clicked, this, &RibbonContentWindow::requestNewWindow);

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
		connect(newWindow, &QAction::triggered, this, &RibbonContentWindow::requestNewWindow);

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

		connect(light, &QAction::triggered, this,
				[this, status]()
				{
					emit requestThemeScheme(mps::theme::Scheme::Light);
					status->setText(QStringLiteral("Requesting Fluent Light…"));
				});
		connect(dark, &QAction::triggered, this,
				[this, status]()
				{
					emit requestThemeScheme(mps::theme::Scheme::Dark);
					status->setText(QStringLiteral("Requesting Fluent Dark…"));
				});

		Q_UNUSED(bridge);
		qfluentribbon::ScreenTip::install(this);
	}
} // namespace mps::demo
