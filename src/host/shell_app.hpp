#pragma once

#include "client_session.hpp"
#include "shell_window.hpp"
#include "tear_out_preview.hpp"

#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <QPropertyAnimation>
#include <QTimer>

#include <memory>
#include <vector>

namespace mps::host
{
	class ShellApp final : public QObject
	{
		Q_OBJECT
	public:
		explicit ShellApp(QString clientExe, QObject* parent = nullptr);
		~ShellApp() override;
		[[nodiscard]] ShellWindow* createShell(QPoint pos = {}, QSize size = {}, bool showNow = true);
		void createClientOn(ShellWindow* shell);
		void closeTab(qint64 tabId);
		void activateTab(ShellWindow* shell, qint64 tabId);
		void tearOutTab(ShellWindow* source, qint64 tabId, QRect suggestedGeometry);
		void mergeTab(qint64 tabId, ShellWindow* target, int insertIndex = -1);
		void closeShell(ShellWindow* shell);
		void clearAllDropIndicators();
		void beginTabDrag(ShellWindow* source, qint64 tabId, QPoint localHotSpot = {});
		void noteTabDragDropHandled();
		[[nodiscard]] bool consumeDragCancelled();
		[[nodiscard]] bool shouldSuppressTearOutAt(QPoint globalPos) const;
		[[nodiscard]] QRect tearOutPreviewGeometry() const;
		void endTabDrag(bool tearOrMerge);
		[[nodiscard]] ShellWindow* shellForTab(qint64 tabId) const;
		[[nodiscard]] ShellWindow* shellAtGlobal(QPoint globalPos) const;
		[[nodiscard]] ShellWindow* shellFromStripDropTarget(QObject* watched) const;
		[[nodiscard]] ShellWindow* tabDropZoneShellAtGlobal(QPoint globalPos) const;
		void destroyShellIfEmpty(ShellWindow* shell);

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override;

	private:
		void onNewConnection();
		void bindShell(ShellWindow* shell);
		void onSessionReady(ClientSession* session);
		void onSubWindowAdded(ClientSession* session, qint64 tabId, QString title, quintptr wid);
		void onSubWindowRemoved(ClientSession* session, qint64 tabId);
		void onSessionDead(ClientSession* session);
		void updateTabDragVisuals();
		void clearAllTabYieldPreviews();
		void pollEscapeCancel();
		void startGhostSnapBack();
		void finishGhostSnapBack();
		[[nodiscard]] QString makeTitle(int clientIndex, int windowIndex) const;

		QString clientExe_;
		QString endpoint_;
		QString token_;
		QLocalServer* server_ = nullptr;
		std::vector<std::unique_ptr<ShellWindow>> shells_;
		std::vector<std::unique_ptr<ClientSession>> sessions_;
		QHash<qint64, ShellWindow*> tabToShell_;
		qint64 nextTabId_ = 1;
		int nextClientIndex_ = 1;
		QHash<int, int> nextWindowIndex_; // clientIndex -> next M
		// Queued first CreateSubWindow per session after ready
		QHash<ClientSession*, ShellWindow*> pendingFirstShell_;

		// Detachable-tab tear-out drag session
		TearOutPreview* tearOutPreview_ = nullptr;
		TabDragGhost* tabDragGhost_ = nullptr;
		QTimer* dragVisualTimer_ = nullptr;
		QPropertyAnimation* ghostSnapAnim_ = nullptr;
		ShellWindow* dragSource_ = nullptr;
		qint64 dragTabId_ = 0;
		qint64 dragResumeTabId_ = 0;
		QPoint dragHotSpot_{40, 20};
		QPoint tabGhostHotSpot_{20, 16};
		QSize dragPreviewSize_{720, 480};
		int dragTabWidth_ = 0;
		bool dragDropHandled_ = false;
		bool dragActive_ = false;
		bool dragCancelled_ = false;
		bool tearOutDetached_ = false; // hysteresis: window-preview mode
		bool ghostSnapBackActive_ = false;
		bool dragForbiddenCursor_ = false;
		// Leave strip → tear-out; return requires getting closer (hysteresis).
		// Slops live in mps::tab_strip (unit-tested).
	};
} // namespace mps::host
