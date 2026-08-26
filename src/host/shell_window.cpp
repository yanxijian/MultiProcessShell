#include "shell_window.hpp"

#include "shell_app.hpp"
#include "tab_strip.hpp"

#include <QAction>
#include <QApplication>
#include <QBitmap>
#include <QCloseEvent>
#include <QCursor>
#include <QDrag>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace mps::host
{
	namespace
	{
		qint64 g_nextShellId = 1;
		constexpr int kTabStripTop = 4;
		constexpr int kTabSlideMs = 120;
		constexpr int kTabCornerRadius = 4;
		constexpr int kTabButtonMaxWidth = 200;
		constexpr int kTabButtonMinWidth = 72;
		constexpr int kDefaultWindowRadius = 8;
		constexpr int kDefaultWindowBorderWidth = 1;
		constexpr auto kPropWindowRadius = "qtheme.window.radius";
		constexpr auto kPropWindowBorderWidth = "qtheme.window.borderWidth";
		constexpr auto kPropWindowBorder = "qtheme.window.border";

		/// Plain arrow for QDrag::setDragCursor. QCursor(Arrow).pixmap() is often null
		/// on Windows, which leaves the native OLE "Move" badge (small right arrow).
		[[nodiscard]] QPixmap plainArrowDragCursorPixmap()
		{
			const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
			const int dim = int(std::ceil(32 * dpr));
			QPixmap pm(dim, dim);
			pm.fill(Qt::transparent);
			pm.setDevicePixelRatio(dpr);
			QPainter p(&pm);
			p.setRenderHint(QPainter::Antialiasing, true);
			QPainterPath path;
			path.moveTo(1.0, 1.0);
			path.lineTo(1.0, 21.0);
			path.lineTo(5.5, 16.5);
			path.lineTo(9.5, 26.0);
			path.lineTo(12.5, 24.5);
			path.lineTo(8.0, 15.0);
			path.lineTo(15.0, 15.0);
			path.closeSubpath();
			p.setPen(QPen(QColor(0, 0, 0), 1.15));
			p.setBrush(QColor(255, 255, 255));
			p.drawPath(path);
			return pm;
		}

		/// Window frame stroke. Prefer theme hint; dark falls back to Fluent `button.border`.
		[[nodiscard]] QColor windowFrameStroke(const QColor& fill, const QColor& hint = QColor())
		{
			const bool dark = fill.lightness() < 128;
			QColor border = hint.isValid() ? hint : QColor();
			if (!border.isValid())
			{
				// Match QPushButton chrome (`button.border` in Fluent packs).
				border = dark ? QColor(0x3f, 0x3f, 0x3f) : QColor(0xd1, 0xd1, 0xd1);
			}
			border.setAlpha(255);
			return border;
		}

		/// Same stroke as QThemeStyle QPushButton (`button.border` in Fluent packs).
		[[nodiscard]] QColor homeTabStroke(const QColor& windowFill)
		{
			return windowFrameStroke(windowFill);
		}

		/// Tab bar separator — quieter than Home tab / window frame.
		[[nodiscard]] QColor softDividerStroke(const QColor& fill)
		{
			const int g = fill.lightness() < 128 ? fill.lightness() + 28 : fill.lightness() - 18;
			const int v = qBound(0, g, 255);
			return QColor(v, v, v);
		}

		/// 1-bit round silhouette for an opaque (non-layered) top-level HWND.
		/// Cuts the square-corner fill that otherwise shows past the rounded stroke.
		[[nodiscard]] QBitmap roundedWindowMask(const QSize& size, int radius)
		{
			QBitmap bm(size);
			bm.fill(Qt::color0);
			if (size.width() <= 0 || size.height() <= 0)
			{
				return bm;
			}
			QPainter p(&bm);
			p.setRenderHint(QPainter::Antialiasing, false);
			p.setPen(Qt::NoPen);
			p.setBrush(Qt::color1);
			p.drawRoundedRect(0, 0, size.width() - 1, size.height() - 1, radius, radius);
			return bm;
		}

		/// Central root paints the frame stroke in its own gutter. No sibling overlay:
		/// a full-window overlay is treated as opaque by Qt and covers the native
		/// SetParent embed HWND (blank client). Keep shell non-layered (no translucent).
		class ChromeRoot final : public QWidget
		{
		public:
			explicit ChromeRoot(QWidget* parent)
				: QWidget(parent)
			{
				setAttribute(Qt::WA_TranslucentBackground, false);
				setAutoFillBackground(true);
				setBackgroundRole(QPalette::Window);
			}

			void syncChrome(int newRadius, int newBorderWidth, const QColor& color)
			{
				m_radius = newRadius;
				m_borderWidth = newBorderWidth;
				m_borderColor = color;
				update();
			}

		protected:
			void paintEvent(QPaintEvent* event) override
			{
				QWidget::paintEvent(event);
				if (m_borderWidth <= 0 || width() <= 0 || height() <= 0)
				{
					return;
				}
				QPainter painter(this);
				// No AA: opaque shell cannot blend against desktop; AA fringe looks like leak.
				painter.setRenderHint(QPainter::Antialiasing, false);
				const int inset = qMax(0, m_borderWidth - 1);
				const QRect r = rect().adjusted(inset, inset, -inset - 1, -inset - 1);
				QPen pen(m_borderColor, 1);
				pen.setJoinStyle(Qt::MiterJoin);
				painter.setPen(pen);
				painter.setBrush(Qt::NoBrush);
				if (m_radius > 0)
				{
					painter.drawRoundedRect(r, m_radius, m_radius);
				}
				else
				{
					painter.drawRect(r);
				}
			}

		private:
			int m_radius = 0;
			int m_borderWidth = 0;
			QColor m_borderColor{0xd1, 0xd1, 0xd1};
		};

		void clearStyleSheet(QWidget* widget)
		{
			if (widget && !widget->styleSheet().isEmpty())
			{
				widget->setStyleSheet(QString());
			}
		}

		void applyPaletteFill(QWidget* widget, const QPalette& pal, QPalette::ColorRole role)
		{
			if (!widget)
			{
				return;
			}
			clearStyleSheet(widget);
			widget->setPalette(pal);
			widget->setBackgroundRole(role);
			widget->setAutoFillBackground(true);
		}
	} // namespace

	TabButton::TabButton(const TabInfo& info, QWidget* parent)
		: QFrame(parent)
		, m_info(info)
	{
		setObjectName(QStringLiteral("TabButton"));
		setFrameShape(QFrame::NoFrame);
		setCursor(Qt::ArrowCursor);
		setAttribute(Qt::WA_Hover, true);
		setAttribute(Qt::WA_StyledBackground, false);
		if (!m_info.isHome)
		{
			setMinimumWidth(kTabButtonMinWidth);
			setMaximumWidth(kTabButtonMaxWidth);
		}
		auto* lay = new QHBoxLayout(this);
		lay->setContentsMargins(10, 4, 6, 4);
		lay->setSpacing(6);
		m_title = new QLabel(this);
		m_title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		lay->addWidget(m_title, 1);
		if (!m_info.isHome)
		{
			m_closeBtn = new QPushButton(QStringLiteral("×"), this);
			m_closeBtn->setFixedSize(18, 18);
			m_closeBtn->setFlat(true);
			m_closeBtn->setCursor(Qt::ArrowCursor);
			m_closeBtn->setFocusPolicy(Qt::NoFocus);
			// Middle-click on × should close too (button otherwise swallows the event).
			m_closeBtn->installEventFilter(this);
			lay->addWidget(m_closeBtn);
			connect(m_closeBtn, &QPushButton::clicked, this,
					[this]
					{
						emit closeRequested(m_info.tabId);
					});
		}
		refreshChrome();
		updateElidedTitle();
	}

	void TabButton::setInfo(const TabInfo& info)
	{
		m_info = info;
		updateElidedTitle();
		refreshChrome();
	}

	void TabButton::updateElidedTitle()
	{
		if (!m_title)
		{
			return;
		}
		const QString full = m_info.displayTitle();
		setToolTip(full);
		m_title->setToolTip(full);
		if (m_info.isHome)
		{
			m_title->setText(full);
			return;
		}
		const QMargins margins = layout() ? layout()->contentsMargins() : QMargins();
		const int spacing = layout() ? layout()->spacing() : 0;
		const int closeW = m_closeBtn ? (m_closeBtn->width() + spacing) : 0;
		const int avail = qMax(0, width() - margins.left() - margins.right() - closeW);
		m_title->setText(QFontMetrics(m_title->font()).elidedText(full, Qt::ElideMiddle, avail));
	}

	void TabButton::resizeEvent(QResizeEvent* event)
	{
		QFrame::resizeEvent(event);
		updateElidedTitle();
	}

	QSize TabButton::sizeHint() const
	{
		const QSize base = QFrame::sizeHint();
		if (m_info.isHome)
		{
			return base;
		}
		const QMargins margins = layout() ? layout()->contentsMargins() : QMargins(10, 4, 6, 4);
		const int spacing = layout() ? layout()->spacing() : 6;
		const int closeW = m_closeBtn ? (18 + spacing) : 0;
		const int textW = QFontMetrics(font()).horizontalAdvance(m_info.displayTitle());
		const int ideal = margins.left() + margins.right() + closeW + textW;
		return QSize(qBound(ideal, kTabButtonMinWidth, kTabButtonMaxWidth), base.height());
	}

	void TabButton::updateTitlePalette()
	{
		if (!m_title)
		{
			return;
		}
		const QPalette appPal = QApplication::palette();
		QPalette titlePal = m_title->palette();
		const QColor fg = m_active ? appPal.color(QPalette::Text) : appPal.color(QPalette::WindowText);
		titlePal.setColor(QPalette::WindowText, fg);
		titlePal.setColor(QPalette::Text, fg);
		m_title->setPalette(titlePal);
	}

	void TabButton::refreshChrome()
	{
		// Owner-drawn chrome (no QSS): keep QPushButton/QLabel on QThemeStyle when demo applies QTE.
		clearStyleSheet(this);
		clearStyleSheet(m_title);
		clearStyleSheet(m_closeBtn);
		const QPalette pal = QApplication::palette();
		setPalette(pal);
		if (m_closeBtn)
		{
			m_closeBtn->setPalette(pal);
		}
		updateTitlePalette();
		update();
	}

	void TabButton::setActive(bool on)
	{
		if (m_active == on)
		{
			return;
		}
		m_active = on;
		updateTitlePalette();
		update();
	}

	void TabButton::paintEvent(QPaintEvent* event)
	{
		Q_UNUSED(event);
		const QPalette pal = QApplication::palette();
		const QColor tabBg = pal.color(QPalette::Window);
		const QColor tabSelected = pal.color(QPalette::Base);
		const bool dark = tabBg.lightness() < 128;
		QColor idleBg = tabBg;
		if (!m_info.isHome && m_info.unhealthy)
		{
			idleBg = dark ? QColor(0x3d, 0x34, 0x20) : QColor(0xff, 0xf4, 0xe0);
		}
		const QColor fill = m_active ? tabSelected : idleBg;

		QColor accent;
		qreal penWidth = 1.0;
		if (m_info.isHome)
		{
			accent = homeTabStroke(tabBg);
			penWidth = 1.0;
		}
		else
		{
			accent =
				m_info.unhealthy ? QColor(180, 120, 20) : ((m_info.instanceIndex % 2 == 0) ? QColor(200, 60, 60) : QColor(120, 70, 180));
			penWidth = 1.5;
		}

		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
		QPainterPath path;
		path.addRoundedRect(r, kTabCornerRadius, kTabCornerRadius);
		painter.fillPath(path, fill);
		painter.setPen(QPen(accent, penWidth));
		painter.drawPath(path);
	}

	void TabButton::mousePressEvent(QMouseEvent* event)
	{
		// Middle-click closes a tab (Home is never closable).
		if (event->button() == Qt::MiddleButton)
		{
			if (!m_info.isHome)
			{
				emit closeRequested(m_info.tabId);
			}
			event->accept();
			return;
		}
		if (event->button() == Qt::RightButton)
		{
			if (!m_info.isHome && m_info.unhealthy && m_info.session)
			{
				QMenu menu(this);
				QAction* terminate = menu.addAction(QStringLiteral("终止进程"));
				if (menu.exec(event->globalPosition().toPoint()) == terminate)
				{
					emit terminateSessionRequested(m_info.session);
				}
			}
			event->accept();
			return;
		}
		if (event->button() == Qt::LeftButton)
		{
			m_dragStart = event->pos();
			m_pressActive = true;
			m_dragging = false;
			emit activated(m_info.tabId);
			event->accept();
			return;
		}
		QFrame::mousePressEvent(event);
	}

	void TabButton::mouseMoveEvent(QMouseEvent* event)
	{
		if (!m_pressActive || m_info.isHome || !(event->buttons() & Qt::LeftButton))
		{
			QFrame::mouseMoveEvent(event);
			return;
		}
		if ((event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance())
		{
			event->accept();
			return;
		}
		if (!m_dragging)
		{
			m_dragging = true;
			emit dragStarted(m_info.tabId, m_dragStart);
		}
		event->accept();
	}

	void TabButton::mouseReleaseEvent(QMouseEvent* event)
	{
		m_pressActive = false;
		m_dragging = false;
		setCursor(Qt::ArrowCursor);
		QFrame::mouseReleaseEvent(event);
	}

	bool TabButton::eventFilter(QObject* watched, QEvent* event)
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::MiddleButton && !m_info.isHome)
			{
				emit closeRequested(m_info.tabId);
				return true;
			}
		}
		return QFrame::eventFilter(watched, event);
	}

	ShellWindow::ShellWindow(ShellApp* app, QWidget* parent)
		: QMainWindow(parent)
		, m_app(app)
		, m_shellId(g_nextShellId++)
	{
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		// Opaque top-level only. Layered/translucent + SetParent embed = blank/hang.
		// Round outer corners use 1-bit setMask (no alpha); stroke is painted by ChromeRoot.
		setAttribute(Qt::WA_TranslucentBackground, false);
		setAutoFillBackground(true);
		setBackgroundRole(QPalette::Window);
		clearMask();
		setMinimumSize(720, 480);
		resize(960, 640);
		setWindowTitle(QStringLiteral("Shell"));

		auto* root = new ChromeRoot(this);
		m_root = root;
		setCentralWidget(m_root);
		m_rootLay = new QVBoxLayout(m_root);
		m_rootLay->setContentsMargins(0, 0, 0, 0);
		m_rootLay->setSpacing(0);

		m_titleBar = new QWidget(m_root);
		m_titleBar->setObjectName(QStringLiteral("TitleBar"));
		m_titleBar->setFixedHeight(40);
		auto* titleLay = new QHBoxLayout(m_titleBar);
		titleLay->setContentsMargins(8, 4, 8, 4);
		titleLay->setSpacing(6);

		m_tabRow = new QHBoxLayout();
		m_tabRow->setSpacing(6);
		m_tabRow->setContentsMargins(0, 0, 0, 0);
		titleLay->addLayout(m_tabRow, 0);

		// Blank trail after tabs: drop-to-append + system-move (not window buttons).
		m_tabDropTrail = new QWidget(m_titleBar);
		m_tabDropTrail->setObjectName(QStringLiteral("TabDropTrail"));
		m_tabDropTrail->setMinimumWidth(48);
		m_tabDropTrail->setCursor(Qt::ArrowCursor);
		titleLay->addWidget(m_tabDropTrail, 1);
		m_tabDropTrail->installEventFilter(this);

		auto* minBtn = new QPushButton(QStringLiteral("—"), m_titleBar);
		auto* maxBtn = new QPushButton(QStringLiteral("□"), m_titleBar);
		auto* closeBtn = new QPushButton(QStringLiteral("×"), m_titleBar);
		m_minBtn = minBtn;
		m_maxBtn = maxBtn;
		m_closeBtn = closeBtn;
		for (auto* b : {minBtn, maxBtn, closeBtn})
		{
			b->setFixedSize(28, 24);
			b->setFlat(true);
			b->setFocusPolicy(Qt::NoFocus);
			titleLay->addWidget(b);
		}
		connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
		connect(maxBtn, &QPushButton::clicked, this,
				[this]
				{
					isMaximized() ? showNormal() : showMaximized();
				});
		connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

		auto* titleSep = new QFrame(m_root);
		titleSep->setObjectName(QStringLiteral("TitleBarSep"));
		titleSep->setFrameShape(QFrame::NoFrame);
		titleSep->setFixedHeight(1);
		m_titleBarSep = titleSep;

		m_stack = new QStackedWidget(m_root);
		m_homeSlot = new QWidget(m_stack);
		auto* homeLay = new QVBoxLayout(m_homeSlot);
		homeLay->setContentsMargins(0, 0, 0, 0);
		homeLay->setSpacing(0);
		m_embed = new EmbedContainer(m_stack);
		m_stack->addWidget(m_homeSlot);
		m_stack->addWidget(m_embed);

		m_rootLay->addWidget(m_titleBar);
		m_rootLay->addWidget(titleSep);
		m_rootLay->addWidget(m_stack, 1);

		m_tabs.push_back(TabInfo::makeHome());
		m_activeTabId = kHomeTabId;
		m_activationHistory = {kHomeTabId};
		rebuildTabs();
		syncWorkspace();
		applyThemeChrome();
		setAcceptDrops(true);
	}

	void ShellWindow::setHomeContent(QWidget* content)
	{
		if (!m_homeSlot)
		{
			return;
		}
		QLayout* lay = m_homeSlot->layout();
		if (!lay)
		{
			lay = new QVBoxLayout(m_homeSlot);
			lay->setContentsMargins(0, 0, 0, 0);
			lay->setSpacing(0);
		}
		while (QLayoutItem* item = lay->takeAt(0))
		{
			if (QWidget* w = item->widget())
			{
				w->deleteLater();
			}
			delete item;
		}
		if (content)
		{
			lay->addWidget(content);
		}
		applyThemeChrome();
		syncWorkspace();
	}

	void ShellWindow::applyThemeChrome()
	{
		// Host chrome must not use QSS when Demo installs QThemeStyle: stylesheets
		// steal painting from QTE for QPushButton and leave sticky Light colors.
		const QPalette pal = QApplication::palette();
		setPalette(pal);
		clearStyleSheet(this);
		applyPaletteFill(m_titleBar, pal, QPalette::Window);
		if (m_titleBarSep)
		{
			clearStyleSheet(m_titleBarSep);
			const QColor sep = softDividerStroke(pal.color(QPalette::Window));
			QPalette sepPal = pal;
			sepPal.setColor(QPalette::Window, sep);
			m_titleBarSep->setPalette(sepPal);
			m_titleBarSep->setBackgroundRole(QPalette::Window);
			m_titleBarSep->setAutoFillBackground(true);
		}
		if (m_tabDropTrail)
		{
			// Must stay opaque: transparent trail passes clicks through on some platforms.
			applyPaletteFill(m_tabDropTrail, pal, QPalette::Window);
			m_tabDropTrail->setCursor(Qt::ArrowCursor);
		}
		if (m_root)
		{
			applyPaletteFill(m_root, pal, QPalette::Window);
			m_root->setAttribute(Qt::WA_TranslucentBackground, false);
		}
		applyPaletteFill(m_homeSlot, pal, QPalette::Window);
		applyPaletteFill(m_stack, pal, QPalette::Window);
		for (auto* b : {m_minBtn, m_maxBtn, m_closeBtn})
		{
			if (b)
			{
				clearStyleSheet(b);
				b->setPalette(pal);
			}
		}
		if (m_dropIndicator)
		{
			clearStyleSheet(m_dropIndicator);
			QPalette tip = pal;
			tip.setColor(QPalette::Window, pal.color(QPalette::Highlight));
			m_dropIndicator->setPalette(tip);
			m_dropIndicator->setBackgroundRole(QPalette::Window);
			m_dropIndicator->setAutoFillBackground(true);
		}
		for (TabButton* btn : m_tabButtons)
		{
			if (btn)
			{
				btn->refreshChrome();
			}
		}
		updateFrameChrome();
		update();
	}

	int ShellWindow::frameRadius() const
	{
		if (isMaximized() || isFullScreen())
		{
			return 0;
		}
		const QVariant v = qApp->property(kPropWindowRadius);
		const int radius = v.isValid() ? v.toInt() : kDefaultWindowRadius;
		return radius > 0 ? radius : kDefaultWindowRadius;
	}

	int ShellWindow::frameBorderWidth() const
	{
		if (isMaximized() || isFullScreen())
		{
			return 0;
		}
		const QVariant v = qApp->property(kPropWindowBorderWidth);
		const int width = v.isValid() ? v.toInt() : kDefaultWindowBorderWidth;
		return width > 0 ? width : kDefaultWindowBorderWidth;
	}

	QColor ShellWindow::frameBorderColor() const
	{
		QColor hint;
		const QVariant v = qApp->property(kPropWindowBorder);
		if (v.canConvert<QColor>())
		{
			hint = v.value<QColor>();
		}
		if (!hint.isValid())
		{
			hint = QApplication::palette().color(QPalette::Mid);
		}
		return windowFrameStroke(QApplication::palette().color(QPalette::Window), hint);
	}

	void ShellWindow::updateFrameChrome()
	{
		const int bw = frameBorderWidth();
		const int radius = frameRadius();
		// Gutter ≥ radius so the corner stroke sits outside title/content.
		const int pad = radius > 0 ? qMax(bw, radius) : bw;
		if (m_rootLay)
		{
			m_rootLay->setContentsMargins(pad, pad, pad, pad);
		}
		if (radius > 0)
		{
			setMask(roundedWindowMask(size(), radius));
		}
		else
		{
			clearMask();
		}
		if (auto* root = static_cast<ChromeRoot*>(m_root))
		{
			root->syncChrome(radius, bw, frameBorderColor());
		}
		if (m_tabDropTrail && QWidget::mouseGrabber() == m_tabDropTrail)
		{
			m_tabDropTrail->releaseMouse();
		}
		m_captionMoveActive = false;
		update();
	}

	void ShellWindow::scheduleEmbedResync()
	{
		if (m_embedResyncPending)
		{
			return;
		}
		m_embedResyncPending = true;
		QTimer::singleShot(0, this,
						   [this]
						   {
							   m_embedResyncPending = false;
							   if (m_embed && m_embed->isVisible() && m_embed->has(m_activeTabId))
							   {
								   m_embed->resyncActive();
							   }
						   });
	}

	void ShellWindow::showEvent(QShowEvent* event)
	{
		QMainWindow::showEvent(event);
		updateFrameChrome();
	}

	void ShellWindow::changeEvent(QEvent* event)
	{
		QMainWindow::changeEvent(event);
		if (event && event->type() == QEvent::WindowStateChange)
		{
			updateFrameChrome();
			scheduleEmbedResync();
		}
	}

	void ShellWindow::resizeEvent(QResizeEvent* event)
	{
		QMainWindow::resizeEvent(event);
		updateFrameChrome();
		// Geometry sync is EmbedContainer::resizeEvent — avoid SetParent storms here.
	}

	void ShellWindow::syncWorkspace()
	{
		if (!m_stack)
		{
			return;
		}
		const TabInfo* active = findTab(m_activeTabId);
		const bool showHome = !active || active->isHome || !m_embed || !m_embed->has(m_activeTabId);
		if (showHome)
		{
			m_embed->clearActive(true);
			m_stack->setCurrentWidget(m_homeSlot);
			m_embed->hide();
			m_homeSlot->show();
			m_homeSlot->raise();
		}
		else
		{
			m_stack->setCurrentWidget(m_embed);
			m_embed->show();
			m_embed->activate(m_activeTabId);
			scheduleEmbedResync();
		}
	}

	void ShellWindow::releaseEmbedTrackingForTab(qint64 tabId)
	{
		if (!m_embed)
		{
			return;
		}
		m_embed->releaseActiveIfTab(tabId);
	}

	void ShellWindow::setTabDragHidden(qint64 tabId, bool hidden)
	{
		for (auto* btn : m_tabButtons)
		{
			if (!btn || btn->info().tabId != tabId)
			{
				continue;
			}
			if (hidden)
			{
				auto* eff = new QGraphicsOpacityEffect(btn);
				eff->setOpacity(0.0);
				btn->setGraphicsEffect(eff);
				btn->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			}
			else
			{
				btn->setGraphicsEffect(nullptr);
				btn->setAttribute(Qt::WA_TransparentForMouseEvents, false);
			}
			break;
		}
	}

	QPixmap ShellWindow::grabTabButton(qint64 tabId) const
	{
		for (auto* btn : m_tabButtons)
		{
			if (btn && btn->info().tabId == tabId)
			{
				return btn->grab();
			}
		}
		return {};
	}

	QSize ShellWindow::tabButtonSize(qint64 tabId) const
	{
		for (auto* btn : m_tabButtons)
		{
			if (btn && btn->info().tabId == tabId)
			{
				return btn->size();
			}
		}
		return {};
	}

	void ShellWindow::updateDropInsertIndicator(int insertIndex)
	{
		if (!m_titleBar || m_tabButtons.isEmpty())
		{
			return;
		}
		insertIndex = qBound(1, insertIndex, m_tabs.size());
		if (!m_dropIndicator)
		{
			m_dropIndicator = new QWidget(m_titleBar);
			m_dropIndicator->setObjectName(QStringLiteral("DropInsertIndicator"));
			m_dropIndicator->setFixedWidth(3);
			m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			QPalette tip = QApplication::palette();
			tip.setColor(QPalette::Window, tip.color(QPalette::Highlight));
			m_dropIndicator->setPalette(tip);
			m_dropIndicator->setBackgroundRole(QPalette::Window);
			m_dropIndicator->setAutoFillBackground(true);
		}

		int x = 8;
		if (insertIndex < m_tabButtons.size())
		{
			auto* btn = m_tabButtons[insertIndex];
			if (btn)
			{
				x = btn->geometry().left();
			}
		}
		else if (!m_tabButtons.isEmpty())
		{
			auto* last = m_tabButtons.last();
			if (last)
			{
				x = last->geometry().right() + 2;
			}
		}
		const int y = 4;
		const int h = qMax(8, m_titleBar->height() - 8);
		m_dropIndicator->setGeometry(x - 1, y, 3, h);
		m_dropIndicator->show();
		m_dropIndicator->raise();
	}

	void ShellWindow::clearDropInsertIndicator()
	{
		if (m_dropIndicator)
		{
			m_dropIndicator->hide();
		}
	}

	void ShellWindow::previewTabYieldAtCursor(qint64 dragTabId, QPoint globalPos, int guestWidth, int hotSpotX)
	{
		if (dragTabId == kHomeTabId || !m_tabRow || !m_titleBar)
		{
			return;
		}

		QHash<qint64, TabButton*> byId;
		for (auto* b : m_tabButtons)
		{
			if (b)
			{
				byId.insert(b->info().tabId, b);
			}
		}
		const bool localDrag = byId.contains(dragTabId);
		if (!localDrag && guestWidth <= 0)
		{
			return;
		}
		if (localDrag && m_tabButtons.isEmpty())
		{
			return;
		}

		QVector<qint64> others;
		others.reserve(m_tabs.size());
		for (const auto& t : m_tabs)
		{
			if (!localDrag || t.tabId != dragTabId)
			{
				others.push_back(t.tabId);
			}
		}

		const auto widthOf = [&](qint64 id) -> int
		{
			if (id == dragTabId)
			{
				if (m_dragTabWidth > 0)
				{
					return m_dragTabWidth;
				}
				if (!localDrag && guestWidth > 0)
				{
					return guestWidth;
				}
			}
			auto* btn = byId.value(id, nullptr);
			return btn ? btn->width() : 80;
		};

		Q_UNUSED(globalPos);
		const QPoint cur = QCursor::pos();
		const int minAmong = (!others.isEmpty() && others[0] == kHomeTabId) ? 1 : 0;
		const int dragW = qMax(1, localDrag ? widthOf(dragTabId) : guestWidth);
		const int inset = tab_strip::dragInsetForWidth(dragW);
		const int hsX = hotSpotX >= 0 ? hotSpotX : (dragW / 2);

		const int ghostLeft = cur.x() - hsX;
		const int ghostRight = ghostLeft + dragW;

		int insertAmong = others.size();
		if (m_yieldDragTabId == dragTabId && !m_yieldOrder.isEmpty())
		{
			const int idx = m_yieldOrder.indexOf(dragTabId);
			if (idx >= 0)
			{
				insertAmong = idx;
			}
		}
		else if (localDrag)
		{
			for (int i = 0; i < m_tabs.size(); ++i)
			{
				if (m_tabs[i].tabId == dragTabId)
				{
					insertAmong = i;
					break;
				}
			}
		}

		int originX = m_stripDragOriginX;
		if (originX <= 0)
		{
			for (auto* b : m_tabButtons)
			{
				if (b)
				{
					originX = b->geometry().left();
					break;
				}
			}
		}
		if (originX <= 0)
		{
			originX = tab_strip::kTabStripMargin;
		}

		std::vector<int> otherWidths;
		otherWidths.reserve(static_cast<size_t>(others.size()));
		for (qint64 id : others)
		{
			otherWidths.push_back(widthOf(id));
		}

		// Map pure local centers (origin 0) into title-bar global X.
		const int originGlobalX = m_titleBar->mapToGlobal(QPoint(originX, 0)).x();
		const int localGhostLeft = ghostLeft - originGlobalX;
		const int localGhostRight = ghostRight - originGlobalX;

		insertAmong = tab_strip::computeYieldInsertAmong(otherWidths, dragW, localGhostLeft, localGhostRight, inset, minAmong, insertAmong);

		std::vector<int64_t> othersStd;
		othersStd.reserve(static_cast<size_t>(others.size()));
		for (qint64 id : others)
		{
			othersStd.push_back(id);
		}
		const auto idsStd = tab_strip::buildYieldOrder(othersStd, insertAmong, dragTabId);
		QVector<qint64> ids;
		ids.reserve(static_cast<int>(idsStd.size()));
		for (int64_t id : idsStd)
		{
			ids.push_back(id);
		}

		if (m_yieldDragTabId == dragTabId && m_yieldOrder == ids && m_stripDragLayoutActive)
		{
			return;
		}
		m_yieldDragTabId = dragTabId;
		m_yieldOrder = ids;

		ensureStripDragLayout(localDrag ? dragTabId : 0, localDrag ? 0 : dragW);
		clearDropInsertIndicator();

		int x = m_stripDragOriginX > 0 ? m_stripDragOriginX : tab_strip::kTabStripMargin;
		const int y = tabStripContentY();
		for (qint64 id : ids)
		{
			if (id == dragTabId && !localDrag)
			{
				x += dragW + tab_strip::kTabSpacing;
				continue;
			}
			auto* btn = byId.value(id, nullptr);
			if (!btn)
			{
				continue;
			}
			const int w = (id == dragTabId) ? dragW : widthOf(id);
			const int h = btn->height();
			animateTabGeometry(btn, QRect(x, y, w, h));
			x += w + tab_strip::kTabSpacing;
		}
	}

	int ShellWindow::yieldInsertIndex() const
	{
		if (m_yieldDragTabId == 0 || m_yieldOrder.isEmpty())
		{
			return -1;
		}
		return m_yieldOrder.indexOf(m_yieldDragTabId);
	}

	QRect ShellWindow::tabDragSlotGlobalRect(qint64 tabId) const
	{
		if (!m_titleBar)
		{
			return {};
		}
		const auto widthOf = [this](qint64 id) -> int
		{
			if (id == m_yieldDragTabId && m_dragTabWidth > 0)
			{
				return m_dragTabWidth;
			}
			for (auto* b : m_tabButtons)
			{
				if (b && b->info().tabId == id)
				{
					return b->width();
				}
			}
			return m_dragTabWidth > 0 ? m_dragTabWidth : 80;
		};

		if (m_yieldDragTabId == tabId && !m_yieldOrder.isEmpty())
		{
			int x = m_stripDragOriginX > 0 ? m_stripDragOriginX : tab_strip::kTabStripMargin;
			const int y = tabStripContentY();
			int h = 28;
			for (auto* b : m_tabButtons)
			{
				if (b)
				{
					h = b->height();
					break;
				}
			}
			for (qint64 id : m_yieldOrder)
			{
				const int w = widthOf(id);
				if (id == tabId)
				{
					return QRect(m_titleBar->mapToGlobal(QPoint(x, y)), QSize(w, h));
				}
				x += w + tab_strip::kTabSpacing;
			}
		}

		for (auto* btn : m_tabButtons)
		{
			if (btn && btn->info().tabId == tabId)
			{
				return QRect(btn->mapToGlobal(QPoint(0, 0)), btn->size());
			}
		}
		return {};
	}

	bool ShellWindow::commitTabYieldPreview()
	{
		if (m_yieldDragTabId == 0 || m_yieldOrder.isEmpty())
		{
			return false;
		}
		// Merge guest preview — tab is not in this shell's model.
		if (!findTab(m_yieldDragTabId))
		{
			return false;
		}
		QHash<qint64, TabInfo> byId;
		for (const auto& t : m_tabs)
		{
			byId.insert(t.tabId, t);
		}
		QVector<TabInfo> next;
		next.reserve(m_yieldOrder.size());
		for (qint64 id : m_yieldOrder)
		{
			auto it = byId.find(id);
			if (it != byId.end())
			{
				next.push_back(it.value());
				byId.erase(it);
			}
		}
		for (auto it = byId.begin(); it != byId.end(); ++it)
		{
			next.push_back(it.value());
		}
		const qint64 dragId = m_yieldDragTabId;
		clearTabYieldPreview();
		m_tabs = next;
		rebuildTabs();
		setActiveTab(dragId);
		return true;
	}

	void ShellWindow::ensureStripDragLayout(qint64 hideTabId, int guestWidth)
	{
		if (m_stripDragLayoutActive || !m_tabRow || !m_titleBar)
		{
			if (guestWidth > 0)
			{
				m_dragTabWidth = guestWidth;
			}
			return;
		}
		m_stripDragLayoutActive = true;
		QHash<qint64, QRect> geos;
		m_stripDragOriginX = tab_strip::kTabStripMargin;
		bool originSet = false;
		if (guestWidth > 0)
		{
			m_dragTabWidth = guestWidth;
		}
		for (auto* btn : m_tabButtons)
		{
			if (!btn)
			{
				continue;
			}
			geos.insert(btn->info().tabId, btn->geometry());
			if (!originSet)
			{
				m_stripDragOriginX = btn->geometry().left();
				originSet = true;
			}
			if (hideTabId != 0 && btn->info().tabId == hideTabId && m_dragTabWidth <= 0)
			{
				m_dragTabWidth = btn->width();
			}
		}
		while (QLayoutItem* item = m_tabRow->takeAt(0))
		{
			delete item;
		}
		for (auto* btn : m_tabButtons)
		{
			if (!btn)
			{
				continue;
			}
			btn->setParent(m_titleBar);
			btn->setGeometry(geos.value(btn->info().tabId));
			btn->show();
			btn->raise();
		}
		if (hideTabId != 0)
		{
			setTabDragHidden(hideTabId, true);
		}
		// Catch drops in the open gap (no tab widget there during yield/merge).
		m_titleBar->setAcceptDrops(true);
		if (m_stripDropFilter)
		{
			m_titleBar->installEventFilter(m_stripDropFilter);
		}
	}

	void ShellWindow::animateTabGeometry(TabButton* btn, const QRect& target)
	{
		if (!btn)
		{
			return;
		}
		if (btn->geometry() == target)
		{
			return;
		}
		QPropertyAnimation*& anim = m_tabSlideAnims[btn->info().tabId];
		if (!anim)
		{
			anim = new QPropertyAnimation(btn, "geometry", this);
			anim->setDuration(kTabSlideMs);
			anim->setEasingCurve(QEasingCurve::OutCubic);
		}
		anim->stop();
		anim->setStartValue(btn->geometry());
		anim->setEndValue(target);
		anim->start();
	}

	void ShellWindow::stopTabSlideAnimations()
	{
		for (auto it = m_tabSlideAnims.begin(); it != m_tabSlideAnims.end(); ++it)
		{
			if (it.value())
			{
				it.value()->stop();
				it.value()->deleteLater();
			}
		}
		m_tabSlideAnims.clear();
	}

	void ShellWindow::clearTabYieldPreview()
	{
		stopTabSlideAnimations();
		const qint64 wasDragTab = m_yieldDragTabId;
		const bool had = (m_yieldDragTabId != 0) || !m_yieldOrder.isEmpty() || m_stripDragLayoutActive;
		m_yieldDragTabId = 0;
		m_yieldOrder.clear();
		m_stripDragLayoutActive = false;
		m_dragTabWidth = 0;
		m_stripDragOriginX = 0;
		if (m_titleBar)
		{
			m_titleBar->setAcceptDrops(false);
		}
		if (wasDragTab != 0)
		{
			setTabDragHidden(wasDragTab, false);
		}
		if (!had || !m_tabRow)
		{
			return;
		}
		while (QLayoutItem* item = m_tabRow->takeAt(0))
		{
			delete item;
		}
		for (auto* b : m_tabButtons)
		{
			if (b)
			{
				m_tabRow->addWidget(b);
			}
		}
	}

	void ShellWindow::collapseTornOutTabSlot(qint64 dragTabId)
	{
		// Once the tear-out window preview is up, remaining tabs should close the gap
		// immediately — do not keep an empty slot until mouse release.
		if (dragTabId == 0 || dragTabId == kHomeTabId || !m_titleBar)
		{
			return;
		}

		QHash<qint64, TabButton*> byId;
		for (auto* b : m_tabButtons)
		{
			if (b)
			{
				byId.insert(b->info().tabId, b);
			}
		}
		if (!byId.contains(dragTabId))
		{
			return;
		}

		ensureStripDragLayout(dragTabId, 0);
		setTabDragHidden(dragTabId, true);
		clearDropInsertIndicator();

		std::vector<int64_t> ids;
		ids.reserve(static_cast<size_t>(m_tabs.size()));
		for (const auto& t : m_tabs)
		{
			ids.push_back(t.tabId);
		}
		const auto packedStd = tab_strip::collapseSlotOrder(ids, dragTabId);
		QVector<qint64> packed;
		packed.reserve(static_cast<int>(packedStd.size()));
		for (int64_t id : packedStd)
		{
			packed.push_back(id);
		}
		m_yieldDragTabId = dragTabId;
		m_yieldOrder = packed;

		int x = m_stripDragOriginX > 0 ? m_stripDragOriginX : tab_strip::kTabStripMargin;
		const int y = tabStripContentY();
		for (qint64 id : packed)
		{
			auto* btn = byId.value(id, nullptr);
			if (!btn)
			{
				continue;
			}
			animateTabGeometry(btn, QRect(x, y, btn->width(), btn->height()));
			x += btn->width() + tab_strip::kTabSpacing;
		}
		if (auto* dragBtn = byId.value(dragTabId, nullptr))
		{
			animateTabGeometry(dragBtn, QRect(x, y, 0, dragBtn->height()));
		}
	}

	QRect ShellWindow::tabStripGlobalRect() const
	{
		// During yield, include the reserved drag gap (not only live button rects).
		if (m_stripDragLayoutActive && m_titleBar && !m_yieldOrder.isEmpty())
		{
			const auto widthOf = [this](qint64 id) -> int
			{
				if (id == m_yieldDragTabId && m_dragTabWidth > 0)
				{
					return m_dragTabWidth;
				}
				for (auto* b : m_tabButtons)
				{
					if (b && b->info().tabId == id)
					{
						return b->width();
					}
				}
				return m_dragTabWidth > 0 ? m_dragTabWidth : 80;
			};
			int x = m_stripDragOriginX > 0 ? m_stripDragOriginX : tab_strip::kTabStripMargin;
			int h = 28;
			for (auto* b : m_tabButtons)
			{
				if (b)
				{
					h = b->height();
					break;
				}
			}
			int total = 0;
			for (int i = 0; i < m_yieldOrder.size(); ++i)
			{
				total += widthOf(m_yieldOrder[i]);
				if (i + 1 < m_yieldOrder.size())
				{
					total += tab_strip::kTabSpacing;
				}
			}
			QRect band(m_titleBar->mapToGlobal(QPoint(x, tabStripContentY())), QSize(qMax(total, 1), h));
			if (m_tabDropTrail)
			{
				const QRect r(m_tabDropTrail->mapToGlobal(QPoint(0, 0)), m_tabDropTrail->size());
				band = band.united(r);
			}
			return band;
		}

		QRect band;
		bool any = false;
		for (auto* btn : m_tabButtons)
		{
			if (!btn)
			{
				continue;
			}
			const QRect r(btn->mapToGlobal(QPoint(0, 0)), btn->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (m_tabDropTrail)
		{
			const QRect r(m_tabDropTrail->mapToGlobal(QPoint(0, 0)), m_tabDropTrail->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (!any && m_titleBar)
		{
			band = QRect(m_titleBar->mapToGlobal(QPoint(0, 0)), m_titleBar->size());
		}
		return band;
	}

	int ShellWindow::tabStripContentY() const
	{
		// Resting HBoxLayout vertically centers Preferred-height tabs inside the
		// title-bar margins — do not assume y == kTabStripTop (that looks too high).
		for (auto* btn : m_tabButtons)
		{
			if (!btn || btn->graphicsEffect())
			{
				continue; // skip the opacity-hidden dragged tab
			}
			return btn->y();
		}
		if (m_titleBar)
		{
			const int avail = qMax(1, m_titleBar->height() - 8);
			const int tabH = m_tabButtons.isEmpty() || !m_tabButtons.first() ? 28 : m_tabButtons.first()->height();
			return 4 + qMax(0, (avail - tabH) / 2);
		}
		return kTabStripTop;
	}

	int ShellWindow::tabRowTopGlobal() const
	{
		// Live sibling Y is stable in Y (yield only animates X) and matches resting
		// vertical centering — avoids the ghost sitting a few px above the row.
		for (auto* btn : m_tabButtons)
		{
			if (!btn || btn->graphicsEffect())
			{
				continue;
			}
			return btn->mapToGlobal(QPoint(0, 0)).y();
		}
		if (m_titleBar)
		{
			return m_titleBar->mapToGlobal(QPoint(0, tabStripContentY())).y();
		}
		return QCursor::pos().y();
	}

	int ShellWindow::clientTabCount() const
	{
		int n = 0;
		for (const auto& t : m_tabs)
		{
			if (!t.isHome)
			{
				++n;
			}
		}
		return n;
	}

	bool ShellWindow::isOverWindowButtons(QPoint globalPos) const
	{
		for (auto* b : {m_minBtn, m_maxBtn, m_closeBtn})
		{
			if (!b || !b->isVisible())
			{
				continue;
			}
			const QRect r(b->mapToGlobal(QPoint(0, 0)), b->size());
			if (r.contains(globalPos))
			{
				return true;
			}
		}
		return false;
	}

	bool ShellWindow::isOverTabDropZone(QPoint globalPos) const
	{
		if (!m_titleBar)
		{
			return false;
		}
		// Full strip band (buttons + gaps + trail), not only individual tab widgets.
		const QRect band = tabStripGlobalRect();
		if (band.isValid() && band.adjusted(0, -6, 0, 10).contains(globalPos))
		{
			return true;
		}
		if (m_tabDropTrail)
		{
			const QRect r(m_tabDropTrail->mapToGlobal(QPoint(0, 0)), m_tabDropTrail->size());
			if (r.adjusted(0, -6, 0, 10).contains(globalPos))
			{
				return true;
			}
		}
		return false;
	}

	bool ShellWindow::isNearTabDropZone(QPoint globalPos, int verticalSlop, int horizontalSlop) const
	{
		if (!m_titleBar)
		{
			return false;
		}
		QRect band;
		bool any = false;
		for (auto* btn : m_tabButtons)
		{
			if (!btn)
			{
				continue;
			}
			const QRect r(btn->mapToGlobal(QPoint(0, 0)), btn->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (m_tabDropTrail)
		{
			const QRect r(m_tabDropTrail->mapToGlobal(QPoint(0, 0)), m_tabDropTrail->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (!any)
		{
			const QRect r(m_titleBar->mapToGlobal(QPoint(0, 0)), m_titleBar->size());
			band = r;
		}
		return band.adjusted(-horizontalSlop, -verticalSlop, horizontalSlop, verticalSlop).contains(globalPos);
	}

	bool ShellWindow::isStripDropTarget(const QObject* watched) const
	{
		if (!watched || !m_titleBar)
		{
			return false;
		}
		if (watched == m_tabDropTrail)
		{
			return true;
		}
		// While yielding, titleBar catches drops in the open gap under the ghost.
		if (m_stripDragLayoutActive && watched == m_titleBar)
		{
			return true;
		}
		const auto* w = qobject_cast<const QWidget*>(watched);
		if (!w)
		{
			return false;
		}
		// Tab buttons only — not window min/max/close, not the whole title bar.
		for (auto* btn : m_tabButtons)
		{
			if (btn == w || (btn && btn->isAncestorOf(w)))
			{
				return true;
			}
		}
		return false;
	}

	void ShellWindow::installStripDropFilter(QObject* filter)
	{
		m_stripDropFilter = filter;
		setAcceptDrops(false);
		if (m_titleBar)
		{
			m_titleBar->setAcceptDrops(false);
		}
		reinstallStripDropTargets();
	}

	void ShellWindow::reinstallStripDropTargets()
	{
		if (!m_stripDropFilter || !m_titleBar)
		{
			return;
		}
		// Narrow hot zone: tabs + trailing strip only.
		if (m_tabDropTrail)
		{
			m_tabDropTrail->setAcceptDrops(true);
			m_tabDropTrail->installEventFilter(m_stripDropFilter);
		}
		for (auto* btn : m_tabButtons)
		{
			if (!btn)
			{
				continue;
			}
			btn->setAcceptDrops(true);
			btn->installEventFilter(m_stripDropFilter);
		}
	}

	void ShellWindow::addTab(const TabInfo& info)
	{
		insertTab(info, m_tabs.size());
	}

	void ShellWindow::setTabTitle(qint64 tabId, const QString& title)
	{
		if (tabId == kHomeTabId || title.isEmpty())
		{
			return;
		}
		if (TabInfo* t = findTab(tabId))
		{
			t->title = title;
		}
		for (TabButton* btn : m_tabButtons)
		{
			if (btn && btn->info().tabId == tabId)
			{
				TabInfo info = btn->info();
				info.title = title;
				btn->setInfo(info);
				break;
			}
		}
	}

	void ShellWindow::insertTab(const TabInfo& info, int insertIndex)
	{
		if (info.isHome)
		{
			return;
		}
		// Home stays at index 0; client tabs occupy [1, size].
		insertIndex = tab_strip::clampClientInsertIndex(insertIndex, m_tabs.size());
		m_tabs.insert(insertIndex, info);
		setActiveTab(info.tabId);
		rebuildTabs();
	}

	void ShellWindow::moveTab(qint64 tabId, int insertIndex)
	{
		if (tabId == kHomeTabId)
		{
			return;
		}
		int from = -1;
		for (int i = 0; i < m_tabs.size(); ++i)
		{
			if (m_tabs[i].tabId == tabId)
			{
				from = i;
				break;
			}
		}
		if (from < 0 || m_tabs[from].isHome)
		{
			return;
		}
		insertIndex = tab_strip::clampClientInsertIndex(insertIndex, m_tabs.size());
		if (tab_strip::isNoOpMove(from, insertIndex))
		{
			return; // same slot (before/after self)
		}
		const TabInfo moved = m_tabs.takeAt(from);
		insertIndex = tab_strip::adjustInsertAfterTake(from, insertIndex);
		insertIndex = tab_strip::clampClientInsertIndex(insertIndex, m_tabs.size());
		m_tabs.insert(insertIndex, moved);
		rebuildTabs();
		setActiveTab(tabId);
	}

	int ShellWindow::tabInsertIndexAt(QPoint globalPos) const
	{
		// Midpoint insert: based on cursor X vs packed tab midpoints (not hit-tests).
		if (!m_titleBar || m_tabs.isEmpty())
		{
			return 1;
		}
		std::vector<int> widths;
		widths.reserve(static_cast<size_t>(m_tabs.size()));
		QHash<qint64, int> byId;
		for (auto* btn : m_tabButtons)
		{
			if (btn)
			{
				byId.insert(btn->info().tabId, btn->width());
			}
		}
		for (const auto& t : m_tabs)
		{
			widths.push_back(byId.value(t.tabId, 80));
		}
		const int localX = m_titleBar->mapFromGlobal(globalPos).x();
		return tab_strip::midpointInsertIndex(localX, widths);
	}

	void ShellWindow::removeTab(qint64 tabId)
	{
		if (tabId == kHomeTabId)
		{
			return;
		}
		if (m_embed)
		{
			m_embed->unbind(tabId);
		}
		const bool wasActive = (m_activeTabId == tabId);
		for (int i = 0; i < m_tabs.size(); ++i)
		{
			if (m_tabs[i].tabId == tabId)
			{
				m_tabs.removeAt(i);
				break;
			}
		}
		m_activationHistory.removeAll(tabId);
		if (wasActive)
		{
			const qint64 next = previousActivationTarget(tabId);
			rebuildTabs();
			setActiveTab(next);
			return;
		}
		rebuildTabs();
		syncWorkspace();
	}

	void ShellWindow::setActiveTab(qint64 tabId)
	{
		if (!findTab(tabId))
		{
			tabId = kHomeTabId;
		}
		pushActivationHistory(tabId);
		m_activeTabId = tabId;
		for (auto* b : m_tabButtons)
		{
			b->setActive(b->info().tabId == tabId);
		}
		syncWorkspace();
		if (auto* t = findTab(tabId); t && t->session && !t->isHome)
		{
			t->session->requestActivate(tabId);
			raise();
			activateWindow();
		}
	}

	void ShellWindow::pushActivationHistory(qint64 tabId)
	{
		std::vector<int64_t> hist;
		hist.reserve(static_cast<size_t>(m_activationHistory.size()));
		for (qint64 id : m_activationHistory)
		{
			hist.push_back(id);
		}
		tab_strip::pushMru(hist, tabId);
		m_activationHistory.clear();
		for (int64_t id : hist)
		{
			m_activationHistory.push_back(id);
		}
	}

	qint64 ShellWindow::previousActivationTarget(qint64 closingTabId) const
	{
		std::vector<int64_t> hist;
		hist.reserve(static_cast<size_t>(m_activationHistory.size()));
		for (qint64 id : m_activationHistory)
		{
			hist.push_back(id);
		}
		std::vector<int64_t> existing;
		existing.reserve(static_cast<size_t>(m_tabs.size()));
		for (const auto& t : m_tabs)
		{
			existing.push_back(t.tabId);
		}
		return tab_strip::previousActivationTarget(hist, existing, closingTabId);
	}

	TabInfo* ShellWindow::findTab(qint64 tabId)
	{
		for (auto& t : m_tabs)
		{
			if (t.tabId == tabId)
			{
				return &t;
			}
		}
		return nullptr;
	}

	const TabInfo* ShellWindow::findTab(qint64 tabId) const
	{
		for (const auto& t : m_tabs)
		{
			if (t.tabId == tabId)
			{
				return &t;
			}
		}
		return nullptr;
	}

	void ShellWindow::syncEmbedToActive()
	{
		syncWorkspace();
	}

	void ShellWindow::rebuildTabs()
	{
		while (QLayoutItem* item = m_tabRow->takeAt(0))
		{
			if (auto* w = item->widget())
			{
				w->deleteLater();
			}
			delete item;
		}
		m_tabButtons.clear();
		for (const auto& t : m_tabs)
		{
			auto* btn = new TabButton(t, m_titleBar);
			m_tabButtons.push_back(btn);
			m_tabRow->addWidget(btn);
			btn->setActive(t.tabId == m_activeTabId);
			connect(btn, &TabButton::activated, this, &ShellWindow::tabActivated);
			connect(btn, &TabButton::closeRequested, this, &ShellWindow::tabCloseRequested);
			connect(btn, &TabButton::terminateSessionRequested, this, &ShellWindow::terminateSessionRequested);
			if (!t.isHome)
			{
				connect(
					btn, &TabButton::dragStarted, this,
					[this](qint64 tabId, QPoint localHotSpot)
					{
						if (m_app)
						{
							m_app->beginTabDrag(this, tabId, localHotSpot);
						}
						auto* mime = new QMimeData;
						mime->setData(QString::fromUtf8(kTabMimeType), QByteArray::number(tabId));
						auto* drag = new QDrag(this);
						drag->setMimeData(mime);
						// Invisible drag pixmap — tab ghost / whole-shell follow drawn separately.
						QPixmap empty(1, 1);
						empty.fill(Qt::transparent);
						drag->setPixmap(empty);
						drag->setHotSpot(QPoint(0, 0));
						// Windows OLE ignores QApplication override cursors and uses setDragCursor.
						// Paint our own arrow: system Arrow pixmap is often null, and the native
						// Move cursor adds a small right-arrow badge under the pointer.
						const QPixmap arrowPm = plainArrowDragCursorPixmap();
						drag->setDragCursor(arrowPm, Qt::MoveAction);
						drag->setDragCursor(arrowPm, Qt::CopyAction);
						drag->setDragCursor(arrowPm, Qt::LinkAction);
						// IgnoreAction custom cursors are unsupported on Windows — never rely on ignore.
						drag->setDragCursor(arrowPm, Qt::IgnoreAction);
						QApplication::setOverrideCursor(Qt::ArrowCursor);
						const auto drop = drag->exec(Qt::MoveAction);
						QApplication::restoreOverrideCursor();
						if (m_app && m_app->isDragAutoMerged())
						{
							// Magnetic auto-merge: OLE aborted; endTabDrag runs after settle anim.
							emit dropIndicatorsClearRequested();
							clearDropInsertIndicator();
							if (!m_app->isAutoMergeAnimating())
							{
								m_app->endTabDrag(/*tearOrMerge=*/false);
							}
							return;
						}
						const QRect previewGeom = m_app ? m_app->tearOutPreviewGeometry() : QRect(QCursor::pos() - QPoint(40, 20), size());
						emit dropIndicatorsClearRequested();
						clearDropInsertIndicator();
						if (drop == Qt::IgnoreAction)
						{
							const bool cancelled = m_app && m_app->consumeDragCancelled();
							const QPoint releasePos = QCursor::pos();
							ShellWindow* zoneShell = m_app ? m_app->tabDropZoneShellAtGlobal(releasePos) : nullptr;
							// Release in the open yield gap has no drop widget → IgnoreAction.
							// Same-shell: commit live reorder. Foreign strip: merge.
							if (!cancelled && zoneShell == this && m_app)
							{
								if (!(hasTabYieldPreview() && commitTabYieldPreview()))
								{
									int insertIndex = yieldInsertIndex();
									if (insertIndex < 0)
									{
										insertIndex = tabInsertIndexAt(releasePos);
									}
									moveTab(tabId, insertIndex);
								}
								m_app->noteTabDragDropHandled();
								m_app->endTabDrag(/*tearOrMerge=*/false);
							}
							else if (!cancelled && zoneShell && zoneShell != this && m_app)
							{
								int insertIndex = zoneShell->yieldInsertIndex();
								if (insertIndex < 0)
								{
									insertIndex = zoneShell->tabInsertIndexAt(releasePos);
								}
								m_app->noteTabDragDropHandled();
								m_app->endTabDrag(/*tearOrMerge=*/false);
								m_app->mergeTab(tabId, zoneShell, insertIndex);
							}
							else if (!cancelled && hasTabYieldPreview() && m_app && m_app->shouldSuppressTearOutAt(releasePos))
							{
								// Near the strip with a live yield preview — keep the new order.
								commitTabYieldPreview();
								m_app->noteTabDragDropHandled();
								m_app->endTabDrag(/*tearOrMerge=*/false);
							}
							else if (cancelled || (m_app && m_app->shouldSuppressTearOutAt(releasePos))
									 || (m_app
										 && tab_strip::shouldCancelTearOutOverWindowButtons(m_app->isReleaseOverWindowButtons(releasePos))))
							{
								// Esc, near strip without commit, or over min/max/close → restore.
								if (m_app)
								{
									m_app->endTabDrag(/*tearOrMerge=*/false);
								}
							}
							else
							{
								if (m_app)
								{
									m_app->endTabDrag(/*tearOrMerge=*/true); // keeps preview until tearOut
								}
								emit tabTearOutRequested(tabId, previewGeom);
							}
						}
						else if (m_app)
						{
							// Drop already handled in ShellApp::eventFilter (incl. whole-shell
							// merge redirected by strip geometry). Just end the drag session.
							m_app->endTabDrag(/*tearOrMerge=*/false);
						}
					});
			}
		}
		reinstallStripDropTargets();
	}

	void ShellWindow::takeTabsFrom(ShellWindow* other, const QList<qint64>& tabIds)
	{
		for (qint64 id : tabIds)
		{
			if (id == kHomeTabId)
			{
				continue;
			}
			for (int i = 0; i < other->m_tabs.size(); ++i)
			{
				if (other->m_tabs[i].tabId == id)
				{
					EmbedContainer::transferBinding(other->embedContainer(), m_embed, id);
					m_tabs.push_back(other->m_tabs[i]);
					other->m_tabs.removeAt(i);
					other->m_activationHistory.removeAll(id);
					break;
				}
			}
		}
		if (!other->findTab(other->m_activeTabId))
		{
			other->setActiveTab(other->previousActivationTarget(0));
		}
		else
		{
			other->rebuildTabs();
			other->syncWorkspace();
		}
		rebuildTabs();
		if (clientTabCount() > 0)
		{
			setActiveTab(m_tabs.last().tabId);
		}
		else
		{
			setActiveTab(kHomeTabId);
		}
	}

	void ShellWindow::setSessionUnhealthy(ClientSession* session, bool unhealthy)
	{
		if (!session)
		{
			return;
		}
		for (auto& t : m_tabs)
		{
			if (t.session == session)
			{
				t.unhealthy = unhealthy;
			}
		}
		for (auto* btn : m_tabButtons)
		{
			if (!btn || btn->info().session != session)
			{
				continue;
			}
			TabInfo info = btn->info();
			info.unhealthy = unhealthy;
			btn->setInfo(info);
		}
	}

	void ShellWindow::forceClose()
	{
		m_forceClosing = true;
		clearDropInsertIndicator();
		if (m_embed)
		{
			m_embed->reset();
		}
		close();
	}

	void ShellWindow::closeEvent(QCloseEvent* event)
	{
		if (m_forceClosing)
		{
			QMainWindow::closeEvent(event);
			return;
		}
		// Route through ShellApp so tabs/sessions/shells_ stay consistent.
		event->ignore();
		emit shellCloseRequested(this);
	}

	bool ShellWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
	{
#ifdef Q_OS_WIN
		if (eventType == QByteArrayLiteral("windows_generic_MSG") || eventType == QByteArrayLiteral("windows_dispatcher_MSG"))
		{
			const auto* msg = static_cast<const MSG*>(message);
			const quintptr clientWindow = m_embed ? m_embed->activeClientWindow() : 0;
			if (msg && clientWindow
				&& (msg->message == WM_KEYDOWN || msg->message == WM_KEYUP || msg->message == WM_CHAR || msg->message == WM_SYSKEYDOWN
					|| msg->message == WM_SYSKEYUP || msg->message == WM_SYSCHAR))
			{
				if (msg->message != WM_KEYDOWN || msg->wParam != 'F' || (GetKeyState(VK_CONTROL) & 0x8000) == 0)
				{
					PostMessageW(reinterpret_cast<HWND>(clientWindow), msg->message, msg->wParam, msg->lParam);
					if (result)
					{
						*result = 0;
					}
					return true;
				}
			}
		}
#else
		Q_UNUSED(eventType);
		Q_UNUSED(message);
		Q_UNUSED(result);
#endif
		return QMainWindow::nativeEvent(eventType, message, result);
	}

	bool ShellWindow::eventFilter(QObject* watched, QEvent* event)
	{
		// Drag the frameless window from the trailing tab strip (not tabs / window buttons).
		// Do not grabMouse() — a stuck grab disables Create Client / theme buttons.
		if (watched == m_tabDropTrail)
		{
			if (event->type() == QEvent::MouseButtonPress)
			{
				auto* me = static_cast<QMouseEvent*>(event);
				if (me->button() == Qt::LeftButton && !isMaximized() && !isFullScreen())
				{
					winId();
					if (QWindow* wh = windowHandle())
					{
						wh->startSystemMove();
					}
					else
					{
						m_captionMoveActive = true;
						m_captionMoveOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
					}
					return true;
				}
			}
			else if (event->type() == QEvent::MouseMove && m_captionMoveActive)
			{
				auto* me = static_cast<QMouseEvent*>(event);
				if (me->buttons() & Qt::LeftButton)
				{
					move(me->globalPosition().toPoint() - m_captionMoveOffset);
					return true;
				}
			}
			else if (event->type() == QEvent::MouseButtonRelease && m_captionMoveActive)
			{
				m_captionMoveActive = false;
				return true;
			}
		}
		return QMainWindow::eventFilter(watched, event);
	}
} // namespace mps::host
