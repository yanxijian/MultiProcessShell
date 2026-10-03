#include "shell_window.hpp"

#include "caption_hit_win.hpp"
#include "shell_app.hpp"
#include "tab_strip.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDrag>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPalette>
#include <QRegion>
#include <QPointer>
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
#include <dwmapi.h>
#include <windows.h>
#elif !defined(Q_OS_MACOS)
#include <qpa/qplatformwindow.h>
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

		constexpr int kRoundRgnScale = 4;

		/// 4× AA rounded rect, majority-downsampled to 1-bit scanline spans.
		/// Same silhouette math on every platform; only the apply API differs.
		[[nodiscard]] std::vector<QRect> makeSupersampledRoundRectSpans(int w, int h, int r)
		{
			std::vector<QRect> spans;
			if (w <= 0 || h <= 0)
			{
				return spans;
			}
			if (r <= 0)
			{
				spans.push_back(QRect(0, 0, w, h));
				return spans;
			}
			QImage hi(w * kRoundRgnScale, h * kRoundRgnScale, QImage::Format_ARGB32_Premultiplied);
			if (hi.isNull())
			{
				spans.push_back(QRect(0, 0, w, h));
				return spans;
			}
			hi.fill(0);
			{
				QPainter p(&hi);
				p.setRenderHint(QPainter::Antialiasing, true);
				p.setPen(Qt::NoPen);
				p.setBrush(Qt::white);
				const qreal rr = static_cast<qreal>(r * kRoundRgnScale);
				p.drawRoundedRect(QRectF(0.5, 0.5, hi.width() - 1.0, hi.height() - 1.0), rr, rr);
			}
			spans.reserve(static_cast<size_t>(h));
			const int majority = (kRoundRgnScale * kRoundRgnScale * 255) / 2;
			for (int y = 0; y < h; ++y)
			{
				int x0 = -1;
				for (int x = 0; x <= w; ++x)
				{
					bool on = false;
					if (x < w)
					{
						int sum = 0;
						for (int dy = 0; dy < kRoundRgnScale; ++dy)
						{
							const QRgb* line = reinterpret_cast<const QRgb*>(hi.constScanLine(y * kRoundRgnScale + dy));
							for (int dx = 0; dx < kRoundRgnScale; ++dx)
							{
								sum += qAlpha(line[x * kRoundRgnScale + dx]);
							}
						}
						on = sum > majority;
					}
					if (on)
					{
						if (x0 < 0)
						{
							x0 = x;
						}
					}
					else if (x0 >= 0)
					{
						spans.push_back(QRect(x0, y, x - x0, 1));
						x0 = -1;
					}
				}
			}
			if (spans.empty())
			{
				spans.push_back(QRect(0, 0, w, h));
			}
			return spans;
		}

		[[nodiscard]] QRegion makeSupersampledRoundRectRegion(int w, int h, int r)
		{
			QRegion rg;
			for (const QRect& s : makeSupersampledRoundRectSpans(w, h, r))
			{
				rg += s;
			}
			return rg;
		}

#ifdef Q_OS_WIN
		/// Win10 DWM paints WS_THICKFRAME in the *system* chrome color (light
		/// ~#B4B4B4, or a pale band when the app is dark). Win11-only
		/// DWMWA_BORDER_COLOR is ignored. Turn off DWM NC rendering and sync
		/// immersive dark mode; we draw the 1px stroke ourselves.
		void applyWin32ShellFrame(HWND hwnd, bool dark)
		{
			if (!hwnd)
			{
				return;
			}
			const DWMNCRENDERINGPOLICY ncPolicy = DWMNCRP_DISABLED;
			DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncPolicy, sizeof(ncPolicy));
			const BOOL immersive = dark ? TRUE : FALSE;
			DwmSetWindowAttribute(hwnd, 20, &immersive, sizeof(immersive));
			DwmSetWindowAttribute(hwnd, 19, &immersive, sizeof(immersive));
			const DWORD borderNone = 0xFFFFFFFE;
			DwmSetWindowAttribute(hwnd, 34, &borderNone, sizeof(borderNone));
			const MARGINS margins{0, 0, 0, 0};
			DwmExtendFrameIntoClientArea(hwnd, &margins);
			SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}

		[[nodiscard]] HRGN makeSupersampledRoundRectRgn(int physW, int physH, int physR)
		{
			const std::vector<QRect> spans = makeSupersampledRoundRectSpans(physW, physH, physR);
			if (spans.empty())
			{
				return nullptr;
			}
			std::vector<uchar> buffer(sizeof(RGNDATAHEADER) + spans.size() * sizeof(RECT), 0);
			auto* rd = reinterpret_cast<RGNDATA*>(buffer.data());
			rd->rdh.dwSize = sizeof(RGNDATAHEADER);
			rd->rdh.iType = RDH_RECTANGLES;
			rd->rdh.nCount = static_cast<DWORD>(spans.size());
			rd->rdh.nRgnSize = static_cast<DWORD>(spans.size() * sizeof(RECT));
			rd->rdh.rcBound = RECT{0, 0, physW, physH};
			auto* out = reinterpret_cast<RECT*>(rd->Buffer);
			for (size_t i = 0; i < spans.size(); ++i)
			{
				const QRect& s = spans[i];
				out[i] = RECT{static_cast<LONG>(s.left()), static_cast<LONG>(s.top()),
							  static_cast<LONG>(s.left() + s.width()), static_cast<LONG>(s.top() + s.height())};
			}
			return ExtCreateRegion(nullptr, static_cast<DWORD>(buffer.size()), rd);
		}
#endif

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
				// Always paint the gutter: after activation / embed, Qt may skip
				// auto-fill on the pad band and DWM's system chrome shows through.
				QPainter painter(this);
				const bool round = m_radius > 0;
				painter.setRenderHint(QPainter::Antialiasing, round);
				painter.setPen(Qt::NoPen);
				painter.setBrush(palette().color(backgroundRole()));
				if (round)
				{
					painter.drawRoundedRect(QRectF(rect()), m_radius, m_radius);
				}
				else
				{
					painter.drawRect(rect());
				}
				if (m_borderWidth <= 0 || width() <= 0 || height() <= 0)
				{
					return;
				}
				const qreal bw = qMax(1, m_borderWidth);
				const qreal half = bw / 2.0;
				QRectF r = QRectF(rect()).adjusted(half, half, -half, -half);
				if (!r.isValid())
				{
					return;
				}
				QPen pen(m_borderColor, bw);
				pen.setJoinStyle(Qt::RoundJoin);
				pen.setCapStyle(Qt::RoundCap);
				painter.setPen(pen);
				painter.setBrush(Qt::NoBrush);
				if (round)
				{
					const qreal rad = qMax(0.0, static_cast<qreal>(m_radius) - half);
					painter.drawRoundedRect(r, rad, rad);
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

#if defined(Q_OS_MACOS)
	void applyCocoaWindowRoundClip(WId viewId, qreal radiusPoints, bool enable);
#endif

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
		// Windows: opaque top-level only. Layered/translucent + SetParent embed = blank/hang.
		// macOS: content-view cornerRadius needs a non-opaque NSWindow so the
		// compositor can AA the arc against the desktop.
#ifdef Q_OS_MACOS
		setAttribute(Qt::WA_TranslucentBackground, true);
#else
		setAttribute(Qt::WA_TranslucentBackground, false);
#endif
		setAutoFillBackground(true);
		setBackgroundRole(QPalette::Window);
		clearMask();
		setMinimumSize(720, 480);
		resize(960, 640);
		setWindowTitle(QStringLiteral("Shell"));

#ifdef Q_OS_WIN
		// Windows Aero snap (drag caption to the screen top → maximize, edge
		// halves, Win+arrows) is silently disabled for frameless windows:
		// Qt's FramelessWindowHint creates a plain WS_POPUP
		// (qwindowswindow.cpp WindowCreationData), and the system snap engine
		// requires BOTH WS_MAXIMIZEBOX and WS_THICKFRAME (verified by probe:
		// either style alone → no snap). Restore both — WM_NCCALCSIZE below
		// removes the resize border THICKFRAME would otherwise inset, and NC
		// hit-testing already routes caption/buttons/edges ourselves.
		if (HWND hwnd = reinterpret_cast<HWND>(winId()))
		{
			const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
			SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME);
			applyWin32ShellFrame(hwnd, QApplication::palette().color(QPalette::Window).lightness() < 128);
		}
#endif

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

		// Blank trail after tabs: drop-to-append zone (not window buttons).
		// Window dragging now comes from the NC hit test (caption_hit_win.cpp),
		// not from a mouse filter on this widget.
		m_tabDropTrail = new QWidget(m_titleBar);
		m_tabDropTrail->setObjectName(QStringLiteral("TabDropTrail"));
		m_tabDropTrail->setMinimumWidth(48);
		m_tabDropTrail->setCursor(Qt::ArrowCursor);
		titleLay->addWidget(m_tabDropTrail, 1);

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
		// Window buttons act through WM_NCLBUTTONUP (caption_hit_win.cpp):
		// they report HTMINBUTTON/HTMAXBUTTON/HTCLOSE, so client-side clicked()
		// never fires — no connections are made here.
		// Fallback refresh trigger for the NC caption rect cache.
		m_titleBar->installEventFilter(this);

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
		connect(m_embed, &EmbedContainer::embedHostChanged, this, &ShellWindow::scheduleRenderHeal);

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
		scheduleCaptionHitCacheRefresh(); // initial NC hit-test rect cache
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

	void ShellWindow::refreshNativeFrame()
	{
#ifdef Q_OS_WIN
		QWindow* wh = windowHandle();
		if (!wh)
		{
			return;
		}
		const HWND hwnd = reinterpret_cast<HWND>(wh->winId());
		if (!hwnd)
		{
			return;
		}
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
	}

	void ShellWindow::scheduleRenderHeal()
	{
		if (m_renderHealPending)
		{
			return;
		}
		m_renderHealPending = true;
		QTimer::singleShot(0, this,
						   [this]
						   {
							   m_renderHealPending = false;
							   healRenderSurface();
						   });
	}

	void ShellWindow::healRenderSurface()
	{
		applyNativeWindowRegion();
		// A synchronous full repaint is essential: after a DPI/geometry
		// transient the OS-side damage tracking can report the pad band as
		// clean, so update()-driven repaints never regenerate those pixels.
		// repaint() bypasses the stale damage region and flushes the whole
		// widget rect to the window surface (see class doc for the symptom).
		if (m_root)
		{
			m_root->repaint();
		}
		repaint();
		refreshNativeFrame();
	}

	bool ShellWindow::event(QEvent* event)
	{
		if (event)
		{
			switch (event->type())
			{
				// DPR flip (spurious WM_DPICHANGED churn observed while a
				// client is embedded): Qt re-lays out, but the window surface
				// keeps transient-geometry leftovers in the pad band.
				case QEvent::DevicePixelRatioChange:
				case QEvent::WindowBlocked:
				case QEvent::WindowUnblocked:
				// Alt-tab / switching back from another app: Win10 DWM
				// repaints WS_THICKFRAME in the system chrome color. Tab
				// switches already heal (syncWorkspace); activation must too.
				case QEvent::ActivationChange:
				case QEvent::WindowActivate:
				case QEvent::WindowDeactivate:
					scheduleRenderHeal();
					break;
				default:
					break;
			}
		}
		return QMainWindow::event(event);
	}

	void ShellWindow::updateFrameChrome()
	{
		const int bw = frameBorderWidth();
		const int radius = frameRadius();
		// Gutter ≥ radius: title/stack are square and would cover the rounded
		// stroke if inset is only 1px (broken corners). The gutter is painted
		// Window-color by ChromeRoot so it matches the title bar (not DWM gray).
		const int pad = radius > 0 ? qMax(bw, radius) : bw;
		if (m_rootLay)
		{
			m_rootLay->setContentsMargins(pad, pad, pad, pad);
		}
		applyNativeWindowRegion();
		if (auto* root = static_cast<ChromeRoot*>(m_root))
		{
			root->syncChrome(radius, bw, frameBorderColor());
		}
		scheduleCaptionHitCacheRefresh(); // band thickness / title bar frame may change
		update();
	}

	/// Rounded window clip at native pixels. 4× majority downsample is the
	/// finest staircase a 1-bit region allows (Win32 / X11). macOS uses the
	/// compositor's AA cornerRadius instead. QWidget::setMask is avoided on
	/// HiDPI: QHighDpi::toNativeLocalRegion scales each rect with rounding.
	void ShellWindow::applyNativeWindowRegion()
	{
		// Hidden shells are created off-screen during tear-out; skip the 4×
		// region raster until showEvent so the new window appears sooner.
		if (!isVisible())
		{
			return;
		}
#ifdef Q_OS_WIN
		QWindow* wh = windowHandle();
		if (!wh)
		{
			return;
		}
		const HWND hwnd = reinterpret_cast<HWND>(wh->winId());
		if (!hwnd)
		{
			return;
		}
		applyWin32ShellFrame(hwnd, QApplication::palette().color(QPalette::Window).lightness() < 128);
		const int radius = frameRadius();
		if (radius <= 0)
		{
			// Maximized / fullscreen: full rectangle, no region.
			SetWindowRgn(hwnd, nullptr, TRUE);
			return;
		}
		RECT wr{};
		GetWindowRect(hwnd, &wr);
		const int physW = static_cast<int>(wr.right - wr.left);
		const int physH = static_cast<int>(wr.bottom - wr.top);
		if (physW <= 0 || physH <= 0)
		{
			return;
		}
		const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
		const int physR = qMax(1, qRound(radius * dpr));
		if (HRGN rgn = makeSupersampledRoundRectRgn(physW, physH, physR))
		{
			if (!SetWindowRgn(hwnd, rgn, TRUE))
			{
				DeleteObject(rgn);
			}
		}
#elif defined(Q_OS_MACOS)
		QWindow* wh = windowHandle();
		if (!wh)
		{
			return;
		}
		clearMask();
		const int radius = frameRadius();
		applyCocoaWindowRoundClip(wh->winId(), static_cast<qreal>(radius), radius > 0);
#else
		QWindow* wh = windowHandle();
		if (!wh)
		{
			return;
		}
		const int radius = frameRadius();
		if (radius <= 0)
		{
			clearMask();
			if (QPlatformWindow* pw = wh->handle())
			{
				pw->setMask(QRegion());
			}
			return;
		}
		const qreal dpr = wh->devicePixelRatio() > 0.0 ? wh->devicePixelRatio() : 1.0;
		const int physW = qMax(1, qRound(static_cast<qreal>(wh->width()) * dpr));
		const int physH = qMax(1, qRound(static_cast<qreal>(wh->height()) * dpr));
		const int physR = qMax(1, qRound(static_cast<qreal>(radius) * dpr));
		const QRegion native = makeSupersampledRoundRectRegion(physW, physH, physR);
		if (QPlatformWindow* pw = wh->handle())
		{
			pw->setMask(native);
		}
		else
		{
			setMask(makeSupersampledRoundRectRegion(width(), height(), radius));
		}
#endif
	}

	void ShellWindow::scheduleCaptionHitCacheRefresh()
	{
		if (m_hitCacheRefreshScheduled)
		{
			return;
		}
		m_hitCacheRefreshScheduled = true;
		// Next tick: layout must be activated before child geometry is final.
		QTimer::singleShot(0, this,
						   [this]()
						   {
							   m_hitCacheRefreshScheduled = false;
							   refreshCaptionHitCache();
						   });
	}

	void ShellWindow::refreshCaptionHitCache()
	{
		if (!m_titleBar)
		{
			return;
		}
		// Window-local logical rects — the same space caption_hit_win.cpp
		// converts physical screen coordinates into.
		m_hitTitleBarRect = QRect(m_titleBar->mapTo(this, QPoint(0, 0)), m_titleBar->size());
		const auto windowLocalRect = [this](const QWidget* w) -> QRect
		{
			return w ? QRect(w->mapTo(this, QPoint(0, 0)), w->size()) : QRect();
		};
		m_hitMinRect = windowLocalRect(m_minBtn);
		m_hitMaxRect = windowLocalRect(m_maxBtn);
		m_hitCloseRect = windowLocalRect(m_closeBtn);
		// Tab buttons cover their close buttons (children). Hidden tabs (yield
		// drag) must not keep blocking the caption.
		m_hitInteractiveRects.clear();
		for (TabButton* btn : m_tabButtons)
		{
			if (btn && btn->isVisible())
			{
				m_hitInteractiveRects.push_back(windowLocalRect(btn));
			}
		}
		// Resize grip stays at radius (not the 1px layout stroke) so edges
		// remain hittable. Helpers already return 0 when maximized/fullscreen.
		const int bw = frameBorderWidth();
		const int radius = frameRadius();
		m_hitBandThickness = radius > 0 ? qMax(bw, radius) : bw;
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
			// Maximize/restore transitions churn the DWM frame and the window
			// region; heal the render surface once the state has settled.
			scheduleRenderHeal();
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
			scheduleRenderHeal();
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

	void ShellWindow::setYieldFacePixmap(const QPixmap& pm)
	{
		m_yieldFacePm = pm;
		if (pm.isNull())
		{
			if (m_yieldFace)
			{
				m_yieldFace->hide();
			}
			return;
		}
		syncYieldFace();
	}

	void ShellWindow::syncYieldFace()
	{
		if (!m_titleBar)
		{
			return;
		}
		if (m_yieldFacePm.isNull() && m_app)
		{
			m_yieldFacePm = m_app->tabDragFacePixmap();
		}
		if (m_yieldFacePm.isNull() || m_yieldDragTabId == 0 || !m_yieldOrder.contains(m_yieldDragTabId))
		{
			if (m_yieldFace)
			{
				m_yieldFace->hide();
			}
			return;
		}
		const QRect glob = tabDragSlotGlobalRect(m_yieldDragTabId);
		if (!glob.isValid() || glob.width() < 4)
		{
			if (m_yieldFace)
			{
				m_yieldFace->hide();
			}
			return;
		}
		if (!m_yieldFace)
		{
			m_yieldFace = new QLabel(m_titleBar);
			m_yieldFace->setObjectName(QStringLiteral("TabYieldFace"));
			m_yieldFace->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			m_yieldFace->setAttribute(Qt::WA_TranslucentBackground, true);
			m_yieldFace->setScaledContents(true);
		}
		const QPoint local = m_titleBar->mapFromGlobal(glob.topLeft());
		m_yieldFace->setPixmap(m_yieldFacePm);
		m_yieldFace->setGeometry(QRect(local, glob.size()));
		m_yieldFace->show();
		m_yieldFace->raise();
	}

	void ShellWindow::setTabDragHidden(qint64 tabId, bool hidden)
	{
		for (auto* btn : m_tabButtons)
		{
			if (!btn || btn->info().tabId != tabId)
			{
				continue;
			}
			// hide() rather than opacity 0: a 0-opacity widget still paints a hole
			// when re-inserted into the strip layout (flash on mouse-up tear-out).
			btn->setAttribute(Qt::WA_TransparentForMouseEvents, hidden);
			btn->setVisible(!hidden);
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
		// Detached tear-out: the source must not keep painting the dragged tab
		// (yield face or button). DragMove still hits the old strip otherwise.
		if (m_app && m_app->isTearOutDetached() && m_app->dragSourceWindow() == this)
		{
			clearTabYieldPreview(/*keepDragTabHidden=*/true);
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
		syncYieldFace();
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
			if (hideTabId != 0 && btn->info().tabId == hideTabId)
			{
				btn->hide();
			}
			else
			{
				btn->show();
				btn->raise();
			}
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
		scheduleCaptionHitCacheRefresh(); // strip relayout shifts tab rects / buttons
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
			// Settled geometry feeds the NC hit-test rect cache.
			connect(anim, &QPropertyAnimation::finished, this,
					[this]()
					{
						scheduleCaptionHitCacheRefresh();
					});
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

	void ShellWindow::clearTabYieldPreview(bool keepDragTabHidden)
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
		if (wasDragTab != 0 && !keepDragTabHidden)
		{
			setTabDragHidden(wasDragTab, false);
		}
		if (m_yieldFace)
		{
			m_yieldFace->hide();
		}
		if (!had || !m_tabRow)
		{
			return;
		}
		if (m_titleBar)
		{
			m_titleBar->setUpdatesEnabled(false);
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
		if (m_titleBar)
		{
			m_titleBar->setUpdatesEnabled(true);
		}
		scheduleCaptionHitCacheRefresh(); // restored strip relayout shifts tab rects
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
		if (m_yieldFace)
		{
			m_yieldFace->hide();
		}
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
		syncYieldFace();
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
			if (!btn || !btn->isVisibleTo(m_titleBar))
			{
				continue; // skip the hidden dragged tab
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
			if (!btn || !btn->isVisibleTo(m_titleBar))
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
		if (m_tabRow)
		{
			auto* btn = makeTabButton(info);
			m_tabButtons.insert(insertIndex, btn);
			if (m_stripDragLayoutActive)
			{
				btn->setParent(m_titleBar);
				const int tabW = m_dragTabWidth > 0 ? m_dragTabWidth : qMax(btn->width(), 80);
				int tabH = btn->height();
				QRect gap;
				if (insertIndex > 0)
				{
					if (auto* prev = m_tabButtons[insertIndex - 1])
					{
						const QRect r = prev->geometry();
						gap = QRect(r.x() + r.width() + tab_strip::kTabSpacing, r.y(), tabW, r.height());
					}
				}
				else if (insertIndex + 1 < m_tabButtons.size())
				{
					if (auto* next = m_tabButtons[insertIndex + 1])
					{
						const QRect r = next->geometry();
						gap = QRect(r.x() - tabW - tab_strip::kTabSpacing, r.y(), tabW, r.height());
					}
				}
				if (!gap.isValid())
				{
					if (tabH <= 0)
					{
						tabH = 28;
					}
					gap = QRect(tab_strip::kTabStripMargin, tabStripContentY(), tabW, tabH);
				}
				btn->setGeometry(gap);
				btn->show();
				btn->raise();
			}
			else
			{
				m_tabRow->insertWidget(insertIndex, btn);
			}
		}
		setActiveTab(info.tabId);
		scheduleCaptionHitCacheRefresh();
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
		if (m_embed && m_embed->has(tabId))
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
		removeTabButton(tabId);
		if (wasActive)
		{
			setActiveTab(previousActivationTarget(tabId));
			return;
		}
		syncWorkspace();
		scheduleCaptionHitCacheRefresh();
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

	TabButton* ShellWindow::makeTabButton(const TabInfo& info)
	{
		auto* btn = new TabButton(info, m_titleBar);
		btn->setActive(info.tabId == m_activeTabId);
		connect(btn, &TabButton::activated, this, &ShellWindow::tabActivated);
		connect(btn, &TabButton::closeRequested, this, &ShellWindow::tabCloseRequested);
		connect(btn, &TabButton::terminateSessionRequested, this, &ShellWindow::terminateSessionRequested);
		if (!info.isHome)
		{
			connect(btn, &TabButton::dragStarted, this, &ShellWindow::runTabDrag);
		}
		if (m_stripDropFilter)
		{
			btn->setAcceptDrops(true);
			btn->installEventFilter(m_stripDropFilter);
		}
		return btn;
	}

	void ShellWindow::removeTabButton(qint64 tabId)
	{
		for (int i = 0; i < m_tabButtons.size(); ++i)
		{
			TabButton* btn = m_tabButtons[i];
			if (!btn || btn->info().tabId != tabId)
			{
				continue;
			}
			m_tabButtons.removeAt(i);
			if (m_tabRow)
			{
				m_tabRow->removeWidget(btn);
			}
			btn->setParent(nullptr);
			btn->deleteLater();
			return;
		}
	}

	void ShellWindow::runTabDrag(qint64 tabId, QPoint localHotSpot)
	{
		ShellApp* app = m_app;
		const QPointer<ShellWindow> self(this);
		if (app)
		{
			app->beginTabDrag(this, tabId, localHotSpot);
		}
		auto* mime = new QMimeData;
		mime->setData(QString::fromUtf8(kTabMimeType), QByteArray::number(tabId));
		auto* drag = new QDrag(app ? static_cast<QObject*>(app) : qApp);
		drag->setMimeData(mime);
		QPixmap empty(1, 1);
		empty.fill(Qt::transparent);
		drag->setPixmap(empty);
		drag->setHotSpot(QPoint(0, 0));
		const QPixmap arrowPm = plainArrowDragCursorPixmap();
		drag->setDragCursor(arrowPm, Qt::MoveAction);
		drag->setDragCursor(arrowPm, Qt::CopyAction);
		drag->setDragCursor(arrowPm, Qt::LinkAction);
		drag->setDragCursor(arrowPm, Qt::IgnoreAction);
		QApplication::setOverrideCursor(Qt::ArrowCursor);
		const CaptionHitPauseGuard captionHitPause;
		if (app)
		{
			app->flushDeferredShellDestroysExcept(self.data());
		}
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
#ifdef Q_OS_WIN
		ReleaseCapture();
		if (QWindow* wh = windowHandle(); wh && wh->handle())
		{
			SetCapture(reinterpret_cast<HWND>(wh->winId()));
		}
#endif
		if (app)
		{
			app->beginOleTabDrag();
		}
		const auto drop = drag->exec(Qt::MoveAction);
		if (app)
		{
			app->endOleTabDrag();
		}
		QApplication::restoreOverrideCursor();
		if (!self)
		{
			if (app)
			{
				app->endTabDrag(/*tearOrMerge=*/false);
			}
			return;
		}
		if (m_app && m_app->isDragAutoMerged())
		{
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
			if (m_app && m_app->isTabDragDropHandled())
			{
				m_app->endTabDrag(/*tearOrMerge=*/false);
				return;
			}
			const bool cancelled = m_app && m_app->consumeDragCancelled();
			const QPoint releasePos = QCursor::pos();
			ShellWindow* zoneShell = m_app ? m_app->tabDropZoneShellAtGlobal(releasePos) : nullptr;
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
				m_app->mergeTab(tabId, zoneShell, insertIndex);
				m_app->endTabDrag(/*tearOrMerge=*/false);
			}
			else if (!cancelled && hasTabYieldPreview() && m_app && m_app->shouldSuppressTearOutAt(releasePos))
			{
				commitTabYieldPreview();
				m_app->noteTabDragDropHandled();
				m_app->endTabDrag(/*tearOrMerge=*/false);
			}
			else if (cancelled || (m_app && m_app->shouldSuppressTearOutAt(releasePos))
					 || (m_app && tab_strip::shouldCancelTearOutOverWindowButtons(m_app->isReleaseOverWindowButtons(releasePos))))
			{
				if (m_app)
				{
					m_app->endTabDrag(/*tearOrMerge=*/false);
				}
			}
			else if (!cancelled && m_app && m_app->dragSnapZoneAt(releasePos).zone == snap::Zone::Maximize)
			{
				m_app->noteTearOutMaximizeNext();
				m_app->endTabDrag(/*tearOrMerge=*/true);
				emit tabTearOutRequested(tabId, QRect());
			}
			else
			{
				if (m_app)
				{
					m_app->endTabDrag(/*tearOrMerge=*/true);
				}
				emit tabTearOutRequested(tabId, previewGeom);
			}
		}
		else if (m_app)
		{
			m_app->endTabDrag(/*tearOrMerge=*/false);
		}
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
			auto* btn = makeTabButton(t);
			m_tabButtons.push_back(btn);
			m_tabRow->addWidget(btn);
		}
		reinstallStripDropTargets();
		scheduleCaptionHitCacheRefresh(); // tab set changed: refresh tab + button rects
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
		// Remove the non-client frame that WS_THICKFRAME (added in the ctor for
		// the system snap engine — see there) would otherwise draw: client area
		// == full window rect. Handle both Qt delivery paths and both wParam
		// values — Win10 DefWindowProc otherwise insets ~SM_CXFRAME (~8px) in
		// the system chrome color (light #B4B4B4 / a pale band on a dark app).
		if (eventType == QByteArrayLiteral("windows_generic_MSG") || eventType == QByteArrayLiteral("windows_dispatcher_MSG"))
		{
			const auto* ncMsg = static_cast<const MSG*>(message);
			if (ncMsg && ncMsg->message == WM_NCCALCSIZE)
			{
				if (result)
				{
					*result = 0;
				}
				return true;
			}
			if (ncMsg && ncMsg->message == WM_NCPAINT)
			{
				if (result)
				{
					*result = 0;
				}
				return true;
			}
			// Focus change: DefWindowProc would redraw the DWM/classic NC
			// frame (the same band that vanishes after a tab switch). lParam
			// -1 skips that paint; we still accept the active-state change.
			if (ncMsg && ncMsg->message == WM_NCACTIVATE)
			{
				applyWin32ShellFrame(ncMsg->hwnd,
									 QApplication::palette().color(QPalette::Window).lightness() < 128);
				if (result)
				{
					*result = DefWindowProcW(ncMsg->hwnd, WM_NCACTIVATE, ncMsg->wParam, static_cast<LPARAM>(-1));
				}
				scheduleRenderHeal();
				return true;
			}
			if (ncMsg && ncMsg->message == WM_ACTIVATE)
			{
				applyWin32ShellFrame(ncMsg->hwnd,
									 QApplication::palette().color(QPalette::Window).lightness() < 128);
				scheduleRenderHeal();
			}
			if (ncMsg && (ncMsg->message == WM_DWMCOMPOSITIONCHANGED || ncMsg->message == WM_THEMECHANGED))
			{
				scheduleRenderHeal();
			}
			// Region self-healing: Qt clears the window region on some internal
			// paths (QWindowsWindow::handleDpiChanged -> SetWindowRgn(null))
			// that never reach changeEvent. Re-apply the rounded region one
			// spin later (after Qt finished its own handling).
			if (ncMsg && ncMsg->message == WM_DPICHANGED)
			{
				QTimer::singleShot(0, this,
								   [this]()
								   {
									   updateFrameChrome();
								   });
			}
		}
		// NC hit-test / caption-button adapter first (generic messages only; NC
		// messages never arrive through windows_dispatcher_MSG).
		if (eventType == QByteArrayLiteral("windows_generic_MSG") && nativeCaptionEvent(message, result))
		{
			return true;
		}
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
		// Fallback refresh for the NC caption rect cache: any title bar relayout
		// (tabs added / removed / width change) shifts buttons and tab rects.
		if (watched == m_titleBar && event->type() == QEvent::LayoutRequest)
		{
			scheduleCaptionHitCacheRefresh();
		}
		return QMainWindow::eventFilter(watched, event);
	}
} // namespace mps::host
