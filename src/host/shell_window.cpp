#include "shell_window.hpp"

#include "shell_app.hpp"
#include "tab_strip.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDrag>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <vector>

namespace mps::host
{
	namespace
	{
		qint64 g_nextShellId = 1;
		constexpr int kTabStripTop = 4;
		constexpr int kTabSlideMs = 120;
	} // namespace

	TabButton::TabButton(const TabInfo& info, QWidget* parent)
		: QFrame(parent)
		, info_(info)
	{
		setObjectName(QStringLiteral("TabButton"));
		setFrameShape(QFrame::StyledPanel);
		setCursor(Qt::ArrowCursor);
		setAttribute(Qt::WA_Hover, true);
		auto* lay = new QHBoxLayout(this);
		lay->setContentsMargins(10, 4, 6, 4);
		lay->setSpacing(6);
		title_ = new QLabel(info_.title, this);
		title_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		lay->addWidget(title_);
		if (!info_.isHome)
		{
			auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
			closeBtn->setFixedSize(18, 18);
			closeBtn->setFlat(true);
			closeBtn->setCursor(Qt::ArrowCursor);
			// Middle-click on × should close too (button otherwise swallows the event).
			closeBtn->installEventFilter(this);
			lay->addWidget(closeBtn);
			connect(closeBtn, &QPushButton::clicked, this,
					[this]
					{
						emit closeRequested(info_.tabId);
					});
		}
		if (info_.isHome)
		{
			setStyleSheet(
				QStringLiteral("#TabButton { border: 2px solid #5a5a5a; border-radius: 4px; background: #f3f3f3; }"
							   "#TabButton[active=\"true\"] { background: #ffffff; }"));
		}
		else
		{
			const QColor accent = (info_.clientIndex % 2 == 0) ? QColor(200, 60, 60) : QColor(120, 70, 180);
			setStyleSheet(QStringLiteral("#TabButton { border: 2px solid %1; border-radius: 4px; background: #f3f3f3; }"
										 "#TabButton[active=\"true\"] { background: #ffffff; }")
							  .arg(accent.name()));
		}
	}

	void TabButton::setInfo(const TabInfo& info)
	{
		info_ = info;
		title_->setText(info_.title);
	}

	void TabButton::setActive(bool on)
	{
		setProperty("active", on);
		style()->unpolish(this);
		style()->polish(this);
	}

	void TabButton::mousePressEvent(QMouseEvent* event)
	{
		// Middle-click closes a tab (Home is never closable).
		if (event->button() == Qt::MiddleButton)
		{
			if (!info_.isHome)
			{
				emit closeRequested(info_.tabId);
			}
			event->accept();
			return;
		}
		if (event->button() == Qt::LeftButton)
		{
			dragStart_ = event->pos();
			pressActive_ = true;
			dragging_ = false;
			emit activated(info_.tabId);
			event->accept();
			return;
		}
		QFrame::mousePressEvent(event);
	}

	void TabButton::mouseMoveEvent(QMouseEvent* event)
	{
		if (!pressActive_ || info_.isHome || !(event->buttons() & Qt::LeftButton))
		{
			QFrame::mouseMoveEvent(event);
			return;
		}
		if ((event->pos() - dragStart_).manhattanLength() < QApplication::startDragDistance())
		{
			event->accept();
			return;
		}
		if (!dragging_)
		{
			dragging_ = true;
			emit dragStarted(info_.tabId, dragStart_);
		}
		event->accept();
	}

	void TabButton::mouseReleaseEvent(QMouseEvent* event)
	{
		pressActive_ = false;
		dragging_ = false;
		setCursor(Qt::ArrowCursor);
		QFrame::mouseReleaseEvent(event);
	}

	bool TabButton::eventFilter(QObject* watched, QEvent* event)
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::MiddleButton && !info_.isHome)
			{
				emit closeRequested(info_.tabId);
				return true;
			}
		}
		return QFrame::eventFilter(watched, event);
	}

	ShellWindow::ShellWindow(ShellApp* app, QWidget* parent)
		: QMainWindow(parent)
		, app_(app)
		, shellId_(g_nextShellId++)
	{
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setMinimumSize(720, 480);
		resize(960, 640);
		setWindowTitle(QStringLiteral("MultiProcessShell Demo"));

		auto* root = new QWidget(this);
		setCentralWidget(root);
		auto* rootLay = new QVBoxLayout(root);
		rootLay->setContentsMargins(0, 0, 0, 0);
		rootLay->setSpacing(0);

		titleBar_ = new QWidget(root);
		titleBar_->setObjectName(QStringLiteral("TitleBar"));
		titleBar_->setFixedHeight(40);
		titleBar_->setStyleSheet(
			QStringLiteral("#TitleBar { background: #e8e8e8; border-bottom: 1px solid #c0c0c0; }"));
		auto* titleLay = new QHBoxLayout(titleBar_);
		titleLay->setContentsMargins(8, 4, 8, 4);
		titleLay->setSpacing(6);

		tabRow_ = new QHBoxLayout();
		tabRow_->setSpacing(6);
		tabRow_->setContentsMargins(0, 0, 0, 0);
		titleLay->addLayout(tabRow_, 0);

		// Blank trail after tabs: drop-to-append + system-move (not window buttons).
		tabDropTrail_ = new QWidget(titleBar_);
		tabDropTrail_->setObjectName(QStringLiteral("TabDropTrail"));
		tabDropTrail_->setMinimumWidth(48);
		tabDropTrail_->setCursor(Qt::SizeAllCursor);
		tabDropTrail_->setStyleSheet(QStringLiteral("#TabDropTrail { background: transparent; }"));
		titleLay->addWidget(tabDropTrail_, 1);
		tabDropTrail_->installEventFilter(this);

		auto* minBtn = new QPushButton(QStringLiteral("—"), titleBar_);
		auto* maxBtn = new QPushButton(QStringLiteral("□"), titleBar_);
		auto* closeBtn = new QPushButton(QStringLiteral("×"), titleBar_);
		minBtn_ = minBtn;
		maxBtn_ = maxBtn;
		closeBtn_ = closeBtn;
		for (auto* b : {minBtn, maxBtn, closeBtn})
		{
			b->setFixedSize(28, 24);
			b->setFlat(true);
			titleLay->addWidget(b);
		}
		connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
		connect(maxBtn, &QPushButton::clicked, this,
				[this]
				{
					isMaximized() ? showNormal() : showMaximized();
				});
		connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

		stack_ = new QStackedWidget(root);
		emptyPage_ = new QWidget(stack_);
		auto* emptyLay = new QVBoxLayout(emptyPage_);
		createClientBtn_ = new QPushButton(QStringLiteral("创建 Client"), emptyPage_);
		createClientBtn_->setFixedSize(160, 40);
		emptyLay->addStretch();
		emptyLay->addWidget(createClientBtn_, 0, Qt::AlignCenter);
		emptyLay->addStretch();
		connect(createClientBtn_, &QPushButton::clicked, this, &ShellWindow::createClientClicked);

		embed_ = new EmbedContainer(stack_);
		stack_->addWidget(emptyPage_);
		stack_->addWidget(embed_);

		rootLay->addWidget(titleBar_);
		rootLay->addWidget(stack_, 1);

		tabs_.push_back(TabInfo::makeHome());
		activeTabId_ = kHomeTabId;
		activationHistory_ = {kHomeTabId};
		rebuildTabs();
		syncWorkspace();
		setAcceptDrops(true);
	}

	void ShellWindow::showEmptyState(bool empty)
	{
		Q_UNUSED(empty);
		syncWorkspace();
	}

	void ShellWindow::syncWorkspace()
	{
		if (!stack_)
		{
			return;
		}
		const TabInfo* active = findTab(activeTabId_);
		const bool showHome = !active || active->isHome || active->wid == 0;
		if (showHome)
		{
			embed_->clearForeignWindow(true);
			stack_->setCurrentWidget(emptyPage_);
		}
		else
		{
			stack_->setCurrentWidget(embed_);
			embed_->setForeignWindow(active->wid);
			scheduleEmbedResync();
		}
	}

	void ShellWindow::scheduleEmbedResync()
	{
		QTimer::singleShot(0, this,
						   [this]
						   {
							   if (auto* t = findTab(activeTabId_); t && !t->isHome && t->wid)
							   {
								   embed_->setForeignWindow(t->wid);
								   embed_->resyncForeignWindow();
							   }
						   });
	}

	void ShellWindow::releaseEmbedOwnershipForTab(qint64 tabId)
	{
		if (activeTabId_ != tabId || !embed_)
		{
			return;
		}
		embed_->releaseForeignWindow();
	}

	void ShellWindow::setTabDragHidden(qint64 tabId, bool hidden)
	{
		for (auto* btn : tabButtons_)
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
		for (auto* btn : tabButtons_)
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
		for (auto* btn : tabButtons_)
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
		// Kept for optional debug; live yield (`previewTabYieldAtCursor`) is the
		// Legacy merge/reorder cue — callers no longer show this blue bar.
		if (!titleBar_ || tabButtons_.isEmpty())
		{
			return;
		}
		insertIndex = qBound(1, insertIndex, tabs_.size());
		if (!dropIndicator_)
		{
			dropIndicator_ = new QWidget(titleBar_);
			dropIndicator_->setObjectName(QStringLiteral("DropInsertIndicator"));
			dropIndicator_->setFixedWidth(3);
			dropIndicator_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			dropIndicator_->setStyleSheet(
				QStringLiteral("#DropInsertIndicator { background: #2d6cdf; border-radius: 1px; }"));
		}

		int x = 8;
		if (insertIndex < tabButtons_.size())
		{
			auto* btn = tabButtons_[insertIndex];
			if (btn)
			{
				x = btn->geometry().left();
			}
		}
		else if (!tabButtons_.isEmpty())
		{
			auto* last = tabButtons_.last();
			if (last)
			{
				x = last->geometry().right() + 2;
			}
		}
		const int y = 4;
		const int h = qMax(8, titleBar_->height() - 8);
		dropIndicator_->setGeometry(x - 1, y, 3, h);
		dropIndicator_->show();
		dropIndicator_->raise();
	}

	void ShellWindow::clearDropInsertIndicator()
	{
		if (dropIndicator_)
		{
			dropIndicator_->hide();
		}
	}

	void ShellWindow::previewTabYieldAtCursor(qint64 dragTabId, QPoint globalPos, int guestWidth, int hotSpotX)
	{
		if (dragTabId == kHomeTabId || !tabRow_ || !titleBar_)
		{
			return;
		}

		QHash<qint64, TabButton*> byId;
		for (auto* b : tabButtons_)
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
		if (localDrag && tabButtons_.isEmpty())
		{
			return;
		}

		QVector<qint64> others;
		others.reserve(tabs_.size());
		for (const auto& t : tabs_)
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
				if (dragTabWidth_ > 0)
				{
					return dragTabWidth_;
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
		if (yieldDragTabId_ == dragTabId && !yieldOrder_.isEmpty())
		{
			const int idx = yieldOrder_.indexOf(dragTabId);
			if (idx >= 0)
			{
				insertAmong = idx;
			}
		}
		else if (localDrag)
		{
			for (int i = 0; i < tabs_.size(); ++i)
			{
				if (tabs_[i].tabId == dragTabId)
				{
					insertAmong = i;
					break;
				}
			}
		}

		int originX = stripDragOriginX_;
		if (originX <= 0)
		{
			for (auto* b : tabButtons_)
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
		const int originGlobalX = titleBar_->mapToGlobal(QPoint(originX, 0)).x();
		const int localGhostLeft = ghostLeft - originGlobalX;
		const int localGhostRight = ghostRight - originGlobalX;

		insertAmong = tab_strip::computeYieldInsertAmong(otherWidths, dragW, localGhostLeft, localGhostRight, inset,
														 minAmong, insertAmong);

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

		if (yieldDragTabId_ == dragTabId && yieldOrder_ == ids && stripDragLayoutActive_)
		{
			return;
		}
		yieldDragTabId_ = dragTabId;
		yieldOrder_ = ids;

		ensureStripDragLayout(localDrag ? dragTabId : 0, localDrag ? 0 : dragW);
		clearDropInsertIndicator();

		int x = stripDragOriginX_ > 0 ? stripDragOriginX_ : tab_strip::kTabStripMargin;
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
		if (yieldDragTabId_ == 0 || yieldOrder_.isEmpty())
		{
			return -1;
		}
		return yieldOrder_.indexOf(yieldDragTabId_);
	}

	QRect ShellWindow::tabDragSlotGlobalRect(qint64 tabId) const
	{
		if (!titleBar_)
		{
			return {};
		}
		const auto widthOf = [this](qint64 id) -> int
		{
			if (id == yieldDragTabId_ && dragTabWidth_ > 0)
			{
				return dragTabWidth_;
			}
			for (auto* b : tabButtons_)
			{
				if (b && b->info().tabId == id)
				{
					return b->width();
				}
			}
			return dragTabWidth_ > 0 ? dragTabWidth_ : 80;
		};

		if (yieldDragTabId_ == tabId && !yieldOrder_.isEmpty())
		{
			int x = stripDragOriginX_ > 0 ? stripDragOriginX_ : tab_strip::kTabStripMargin;
			const int y = tabStripContentY();
			int h = 28;
			for (auto* b : tabButtons_)
			{
				if (b)
				{
					h = b->height();
					break;
				}
			}
			for (qint64 id : yieldOrder_)
			{
				const int w = widthOf(id);
				if (id == tabId)
				{
					return QRect(titleBar_->mapToGlobal(QPoint(x, y)), QSize(w, h));
				}
				x += w + tab_strip::kTabSpacing;
			}
		}

		for (auto* btn : tabButtons_)
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
		if (yieldDragTabId_ == 0 || yieldOrder_.isEmpty())
		{
			return false;
		}
		// Merge guest preview — tab is not in this shell's model.
		if (!findTab(yieldDragTabId_))
		{
			return false;
		}
		QHash<qint64, TabInfo> byId;
		for (const auto& t : tabs_)
		{
			byId.insert(t.tabId, t);
		}
		QVector<TabInfo> next;
		next.reserve(yieldOrder_.size());
		for (qint64 id : yieldOrder_)
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
		const qint64 dragId = yieldDragTabId_;
		clearTabYieldPreview();
		tabs_ = next;
		rebuildTabs();
		setActiveTab(dragId);
		return true;
	}

	void ShellWindow::ensureStripDragLayout(qint64 hideTabId, int guestWidth)
	{
		if (stripDragLayoutActive_ || !tabRow_ || !titleBar_)
		{
			if (guestWidth > 0)
			{
				dragTabWidth_ = guestWidth;
			}
			return;
		}
		stripDragLayoutActive_ = true;
		QHash<qint64, QRect> geos;
		stripDragOriginX_ = tab_strip::kTabStripMargin;
		bool originSet = false;
		if (guestWidth > 0)
		{
			dragTabWidth_ = guestWidth;
		}
		for (auto* btn : tabButtons_)
		{
			if (!btn)
			{
				continue;
			}
			geos.insert(btn->info().tabId, btn->geometry());
			if (!originSet)
			{
				stripDragOriginX_ = btn->geometry().left();
				originSet = true;
			}
			if (hideTabId != 0 && btn->info().tabId == hideTabId && dragTabWidth_ <= 0)
			{
				dragTabWidth_ = btn->width();
			}
		}
		while (QLayoutItem* item = tabRow_->takeAt(0))
		{
			delete item;
		}
		for (auto* btn : tabButtons_)
		{
			if (!btn)
			{
				continue;
			}
			btn->setParent(titleBar_);
			btn->setGeometry(geos.value(btn->info().tabId));
			btn->show();
			btn->raise();
		}
		if (hideTabId != 0)
		{
			setTabDragHidden(hideTabId, true);
		}
		// Catch drops in the open gap (no tab widget there during yield/merge).
		titleBar_->setAcceptDrops(true);
		if (stripDropFilter_)
		{
			titleBar_->installEventFilter(stripDropFilter_);
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
		QPropertyAnimation*& anim = tabSlideAnims_[btn->info().tabId];
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
		for (auto it = tabSlideAnims_.begin(); it != tabSlideAnims_.end(); ++it)
		{
			if (it.value())
			{
				it.value()->stop();
				it.value()->deleteLater();
			}
		}
		tabSlideAnims_.clear();
	}

	void ShellWindow::clearTabYieldPreview()
	{
		stopTabSlideAnimations();
		const bool had = (yieldDragTabId_ != 0) || !yieldOrder_.isEmpty() || stripDragLayoutActive_;
		yieldDragTabId_ = 0;
		yieldOrder_.clear();
		stripDragLayoutActive_ = false;
		dragTabWidth_ = 0;
		stripDragOriginX_ = 0;
		if (titleBar_)
		{
			titleBar_->setAcceptDrops(false);
		}
		if (!had || !tabRow_)
		{
			return;
		}
		while (QLayoutItem* item = tabRow_->takeAt(0))
		{
			delete item;
		}
		for (auto* b : tabButtons_)
		{
			if (b)
			{
				tabRow_->addWidget(b);
			}
		}
	}

	void ShellWindow::collapseTornOutTabSlot(qint64 dragTabId)
	{
		// Once the tear-out window preview is up, remaining tabs should close the gap
		// immediately — do not keep an empty slot until mouse release.
		if (dragTabId == 0 || dragTabId == kHomeTabId || !titleBar_)
		{
			return;
		}

		QHash<qint64, TabButton*> byId;
		for (auto* b : tabButtons_)
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
		ids.reserve(static_cast<size_t>(tabs_.size()));
		for (const auto& t : tabs_)
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
		yieldDragTabId_ = dragTabId;
		yieldOrder_ = packed;

		int x = stripDragOriginX_ > 0 ? stripDragOriginX_ : tab_strip::kTabStripMargin;
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
		if (stripDragLayoutActive_ && titleBar_ && !yieldOrder_.isEmpty())
		{
			const auto widthOf = [this](qint64 id) -> int
			{
				if (id == yieldDragTabId_ && dragTabWidth_ > 0)
				{
					return dragTabWidth_;
				}
				for (auto* b : tabButtons_)
				{
					if (b && b->info().tabId == id)
					{
						return b->width();
					}
				}
				return dragTabWidth_ > 0 ? dragTabWidth_ : 80;
			};
			int x = stripDragOriginX_ > 0 ? stripDragOriginX_ : tab_strip::kTabStripMargin;
			int h = 28;
			for (auto* b : tabButtons_)
			{
				if (b)
				{
					h = b->height();
					break;
				}
			}
			int total = 0;
			for (int i = 0; i < yieldOrder_.size(); ++i)
			{
				total += widthOf(yieldOrder_[i]);
				if (i + 1 < yieldOrder_.size())
				{
					total += tab_strip::kTabSpacing;
				}
			}
			QRect band(titleBar_->mapToGlobal(QPoint(x, tabStripContentY())), QSize(qMax(total, 1), h));
			if (tabDropTrail_)
			{
				const QRect r(tabDropTrail_->mapToGlobal(QPoint(0, 0)), tabDropTrail_->size());
				band = band.united(r);
			}
			return band;
		}

		QRect band;
		bool any = false;
		for (auto* btn : tabButtons_)
		{
			if (!btn)
			{
				continue;
			}
			const QRect r(btn->mapToGlobal(QPoint(0, 0)), btn->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (tabDropTrail_)
		{
			const QRect r(tabDropTrail_->mapToGlobal(QPoint(0, 0)), tabDropTrail_->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (!any && titleBar_)
		{
			band = QRect(titleBar_->mapToGlobal(QPoint(0, 0)), titleBar_->size());
		}
		return band;
	}

	int ShellWindow::tabStripContentY() const
	{
		// Resting HBoxLayout vertically centers Preferred-height tabs inside the
		// title-bar margins — do not assume y == kTabStripTop (that looks too high).
		for (auto* btn : tabButtons_)
		{
			if (!btn || btn->graphicsEffect())
			{
				continue; // skip the opacity-hidden dragged tab
			}
			return btn->y();
		}
		if (titleBar_)
		{
			const int avail = qMax(1, titleBar_->height() - 8);
			const int tabH = tabButtons_.isEmpty() || !tabButtons_.first() ? 28 : tabButtons_.first()->height();
			return 4 + qMax(0, (avail - tabH) / 2);
		}
		return kTabStripTop;
	}

	int ShellWindow::tabRowTopGlobal() const
	{
		// Live sibling Y is stable in Y (yield only animates X) and matches resting
		// vertical centering — avoids the ghost sitting a few px above the row.
		for (auto* btn : tabButtons_)
		{
			if (!btn || btn->graphicsEffect())
			{
				continue;
			}
			return btn->mapToGlobal(QPoint(0, 0)).y();
		}
		if (titleBar_)
		{
			return titleBar_->mapToGlobal(QPoint(0, tabStripContentY())).y();
		}
		return QCursor::pos().y();
	}

	int ShellWindow::clientTabCount() const
	{
		int n = 0;
		for (const auto& t : tabs_)
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
		for (auto* b : {minBtn_, maxBtn_, closeBtn_})
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
		if (!titleBar_)
		{
			return false;
		}
		// During live yield the open gap under the ghost has no tab widget — still count
		// as on-strip so merge/reorder do not flip to tear-out.
		if (stripDragLayoutActive_)
		{
			const QRect band = tabStripGlobalRect();
			if (band.isValid() && band.adjusted(0, -4, 0, 4).contains(globalPos))
			{
				return true;
			}
		}
		for (auto* btn : tabButtons_)
		{
			if (!btn || !btn->isVisible())
			{
				continue;
			}
			const QRect r(btn->mapToGlobal(QPoint(0, 0)), btn->size());
			if (r.contains(globalPos))
			{
				return true;
			}
		}
		if (tabDropTrail_)
		{
			const QRect r(tabDropTrail_->mapToGlobal(QPoint(0, 0)), tabDropTrail_->size());
			if (r.contains(globalPos))
			{
				return true;
			}
		}
		return false;
	}

	bool ShellWindow::isNearTabDropZone(QPoint globalPos, int verticalSlop, int horizontalSlop) const
	{
		if (!titleBar_)
		{
			return false;
		}
		QRect band;
		bool any = false;
		for (auto* btn : tabButtons_)
		{
			if (!btn)
			{
				continue;
			}
			const QRect r(btn->mapToGlobal(QPoint(0, 0)), btn->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (tabDropTrail_)
		{
			const QRect r(tabDropTrail_->mapToGlobal(QPoint(0, 0)), tabDropTrail_->size());
			band = any ? band.united(r) : r;
			any = true;
		}
		if (!any)
		{
			const QRect r(titleBar_->mapToGlobal(QPoint(0, 0)), titleBar_->size());
			band = r;
		}
		return band.adjusted(-horizontalSlop, -verticalSlop, horizontalSlop, verticalSlop).contains(globalPos);
	}

	bool ShellWindow::isStripDropTarget(const QObject* watched) const
	{
		if (!watched || !titleBar_)
		{
			return false;
		}
		if (watched == tabDropTrail_)
		{
			return true;
		}
		// While yielding, titleBar catches drops in the open gap under the ghost.
		if (stripDragLayoutActive_ && watched == titleBar_)
		{
			return true;
		}
		const auto* w = qobject_cast<const QWidget*>(watched);
		if (!w)
		{
			return false;
		}
		// Tab buttons only — not window min/max/close, not the whole title bar.
		for (auto* btn : tabButtons_)
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
		stripDropFilter_ = filter;
		setAcceptDrops(false);
		if (titleBar_)
		{
			titleBar_->setAcceptDrops(false);
		}
		reinstallStripDropTargets();
	}

	void ShellWindow::reinstallStripDropTargets()
	{
		if (!stripDropFilter_ || !titleBar_)
		{
			return;
		}
		// Narrow hot zone: tabs + trailing strip only.
		if (tabDropTrail_)
		{
			tabDropTrail_->setAcceptDrops(true);
			tabDropTrail_->installEventFilter(stripDropFilter_);
		}
		for (auto* btn : tabButtons_)
		{
			if (!btn)
			{
				continue;
			}
			btn->setAcceptDrops(true);
			btn->installEventFilter(stripDropFilter_);
		}
	}

	void ShellWindow::addTab(const TabInfo& info)
	{
		insertTab(info, tabs_.size());
	}

	void ShellWindow::insertTab(const TabInfo& info, int insertIndex)
	{
		if (info.isHome)
		{
			return;
		}
		// Home stays at index 0; client tabs occupy [1, size].
		insertIndex = tab_strip::clampClientInsertIndex(insertIndex, tabs_.size());
		tabs_.insert(insertIndex, info);
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
		for (int i = 0; i < tabs_.size(); ++i)
		{
			if (tabs_[i].tabId == tabId)
			{
				from = i;
				break;
			}
		}
		if (from < 0 || tabs_[from].isHome)
		{
			return;
		}
		insertIndex = tab_strip::clampClientInsertIndex(insertIndex, tabs_.size());
		if (tab_strip::isNoOpMove(from, insertIndex))
		{
			return; // same slot (before/after self)
		}
		const TabInfo moved = tabs_.takeAt(from);
		insertIndex = tab_strip::adjustInsertAfterTake(from, insertIndex);
		insertIndex = tab_strip::clampClientInsertIndex(insertIndex, tabs_.size());
		tabs_.insert(insertIndex, moved);
		rebuildTabs();
		setActiveTab(tabId);
	}

	int ShellWindow::tabInsertIndexAt(QPoint globalPos) const
	{
		// Midpoint insert: based on cursor X vs packed tab midpoints (not hit-tests).
		if (!titleBar_ || tabs_.isEmpty())
		{
			return 1;
		}
		std::vector<int> widths;
		widths.reserve(static_cast<size_t>(tabs_.size()));
		QHash<qint64, int> byId;
		for (auto* btn : tabButtons_)
		{
			if (btn)
			{
				byId.insert(btn->info().tabId, btn->width());
			}
		}
		for (const auto& t : tabs_)
		{
			widths.push_back(byId.value(t.tabId, 80));
		}
		const int localX = titleBar_->mapFromGlobal(globalPos).x();
		return tab_strip::midpointInsertIndex(localX, widths);
	}

	void ShellWindow::removeTab(qint64 tabId)
	{
		if (tabId == kHomeTabId)
		{
			return;
		}
		const bool wasActive = (activeTabId_ == tabId);
		for (int i = 0; i < tabs_.size(); ++i)
		{
			if (tabs_[i].tabId == tabId)
			{
				tabs_.removeAt(i);
				break;
			}
		}
		activationHistory_.removeAll(tabId);
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
		activeTabId_ = tabId;
		for (auto* b : tabButtons_)
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
		hist.reserve(static_cast<size_t>(activationHistory_.size()));
		for (qint64 id : activationHistory_)
		{
			hist.push_back(id);
		}
		tab_strip::pushMru(hist, tabId);
		activationHistory_.clear();
		for (int64_t id : hist)
		{
			activationHistory_.push_back(id);
		}
	}

	qint64 ShellWindow::previousActivationTarget(qint64 closingTabId) const
	{
		std::vector<int64_t> hist;
		hist.reserve(static_cast<size_t>(activationHistory_.size()));
		for (qint64 id : activationHistory_)
		{
			hist.push_back(id);
		}
		std::vector<int64_t> existing;
		existing.reserve(static_cast<size_t>(tabs_.size()));
		for (const auto& t : tabs_)
		{
			existing.push_back(t.tabId);
		}
		return tab_strip::previousActivationTarget(hist, existing, closingTabId);
	}

	TabInfo* ShellWindow::findTab(qint64 tabId)
	{
		for (auto& t : tabs_)
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
		for (const auto& t : tabs_)
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
		while (QLayoutItem* item = tabRow_->takeAt(0))
		{
			if (auto* w = item->widget())
			{
				w->deleteLater();
			}
			delete item;
		}
		tabButtons_.clear();
		for (const auto& t : tabs_)
		{
			auto* btn = new TabButton(t, titleBar_);
			tabButtons_.push_back(btn);
			tabRow_->addWidget(btn);
			btn->setActive(t.tabId == activeTabId_);
			connect(btn, &TabButton::activated, this, &ShellWindow::tabActivated);
			connect(btn, &TabButton::closeRequested, this, &ShellWindow::tabCloseRequested);
			if (!t.isHome)
			{
				connect(btn, &TabButton::dragStarted, this,
						[this](qint64 tabId, QPoint localHotSpot)
						{
							if (app_)
							{
								app_->beginTabDrag(this, tabId, localHotSpot);
							}
							auto* mime = new QMimeData;
							mime->setData(QString::fromUtf8(kTabMimeType), QByteArray::number(tabId));
							auto* drag = new QDrag(this);
							drag->setMimeData(mime);
							// Invisible drag pixmap — tab ghost / tear-out preview drawn separately.
							QPixmap empty(1, 1);
							empty.fill(Qt::transparent);
							drag->setPixmap(empty);
							drag->setHotSpot(QPoint(0, 0));
							// Keep normal arrow cursor; blank drag cursors + override.
							drag->setDragCursor(empty, Qt::MoveAction);
							drag->setDragCursor(empty, Qt::CopyAction);
							drag->setDragCursor(empty, Qt::IgnoreAction);
							QApplication::setOverrideCursor(Qt::ArrowCursor);
							const auto drop = drag->exec(Qt::MoveAction);
							QApplication::restoreOverrideCursor();
							const QRect previewGeom =
								app_ ? app_->tearOutPreviewGeometry() : QRect(QCursor::pos() - QPoint(40, 20), size());
							emit dropIndicatorsClearRequested();
							clearDropInsertIndicator();
							if (drop == Qt::IgnoreAction)
							{
								const bool cancelled = app_ && app_->consumeDragCancelled();
								const QPoint releasePos = QCursor::pos();
								ShellWindow* zoneShell = app_ ? app_->tabDropZoneShellAtGlobal(releasePos) : nullptr;
								// Release in the open yield gap has no drop widget → IgnoreAction.
								// Same-shell: commit live reorder. Foreign strip: merge.
								if (!cancelled && zoneShell == this && app_)
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
									app_->noteTabDragDropHandled();
									app_->endTabDrag(/*tearOrMerge=*/false);
								}
								else if (!cancelled && zoneShell && zoneShell != this && app_)
								{
									int insertIndex = zoneShell->yieldInsertIndex();
									if (insertIndex < 0)
									{
										insertIndex = zoneShell->tabInsertIndexAt(releasePos);
									}
									app_->noteTabDragDropHandled();
									app_->endTabDrag(/*tearOrMerge=*/false);
									app_->mergeTab(tabId, zoneShell, insertIndex);
								}
								else if (!cancelled && hasTabYieldPreview() && app_
										 && app_->shouldSuppressTearOutAt(releasePos))
								{
									// Near the strip with a live yield preview — keep the new order.
									commitTabYieldPreview();
									app_->noteTabDragDropHandled();
									app_->endTabDrag(/*tearOrMerge=*/false);
								}
								else if (cancelled || (app_ && app_->shouldSuppressTearOutAt(releasePos)))
								{
									// Esc, or released near strip without a commit path → restore.
									if (app_)
									{
										app_->endTabDrag(/*tearOrMerge=*/false);
									}
								}
								else
								{
									if (app_)
									{
										app_->endTabDrag(/*tearOrMerge=*/true); // keeps preview until tearOut
									}
									emit tabTearOutRequested(tabId, previewGeom);
								}
							}
							else if (app_)
							{
								app_->endTabDrag(/*tearOrMerge=*/false);
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
			for (int i = 0; i < other->tabs_.size(); ++i)
			{
				if (other->tabs_[i].tabId == id)
				{
					tabs_.push_back(other->tabs_[i]);
					other->tabs_.removeAt(i);
					other->activationHistory_.removeAll(id);
					break;
				}
			}
		}
		if (!other->findTab(other->activeTabId_))
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
			setActiveTab(tabs_.last().tabId);
		}
		else
		{
			setActiveTab(kHomeTabId);
		}
	}

	void ShellWindow::forceClose()
	{
		forceClosing_ = true;
		clearDropInsertIndicator();
		if (embed_)
		{
			embed_->releaseForeignWindow();
		}
		close();
	}

	void ShellWindow::closeEvent(QCloseEvent* event)
	{
		if (forceClosing_)
		{
			QMainWindow::closeEvent(event);
			return;
		}
		// Route through ShellApp so tabs/sessions/shells_ stay consistent.
		event->ignore();
		emit shellCloseRequested(this);
	}

	bool ShellWindow::eventFilter(QObject* watched, QEvent* event)
	{
		// System-move on the trailing tab strip (not on Tab buttons / window buttons).
		if (watched == tabDropTrail_ && event->type() == QEvent::MouseButtonPress)
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton)
			{
				winId();
				if (windowHandle())
				{
					windowHandle()->startSystemMove();
				}
				return true;
			}
		}
		return QMainWindow::eventFilter(watched, event);
	}
} // namespace mps::host
