#ifndef __MPS_HOST_SHELL_APP_H__
#define __MPS_HOST_SHELL_APP_H__

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

		QString m_clientExe;
		QString m_endpoint;
		QString m_token;
		QLocalServer* m_server = nullptr;
		std::vector<std::unique_ptr<ShellWindow>> m_shells;
		std::vector<std::unique_ptr<ClientSession>> m_sessions;
		QHash<qint64, ShellWindow*> m_tabToShell;
		qint64 m_nextTabId = 1;
		int m_nextClientIndex = 1;
		QHash<int, int> m_nextWindowIndex; // clientIndex -> next M
		// Queued first CreateSubWindow per session after ready
		QHash<ClientSession*, ShellWindow*> m_pendingFirstShell;

		// Detachable-tab tear-out drag session
		TearOutPreview* m_tearOutPreview = nullptr;
		TabDragGhost* m_tabDragGhost = nullptr;
		QTimer* m_dragVisualTimer = nullptr;
		QPropertyAnimation* m_ghostSnapAnim = nullptr;
		ShellWindow* m_dragSource = nullptr;
		qint64 m_dragTabId = 0;
		qint64 m_dragResumeTabId = 0;
		QPoint m_dragHotSpot{40, 20};
		QPoint m_tabGhostHotSpot{20, 16};
		QSize m_dragPreviewSize{720, 480};
		int m_dragTabWidth = 0;
		bool m_dragDropHandled = false;
		bool m_dragActive = false;
		bool m_dragCancelled = false;
		bool m_tearOutDetached = false; // hysteresis: window-preview mode
		bool m_ghostSnapBackActive = false;
		bool m_dragForbiddenCursor = false;
		// Leave strip → tear-out; return requires getting closer (hysteresis).
		// Slops live in mps::tab_strip (unit-tested).
	};
} // namespace mps::host

#endif  // __MPS_HOST_SHELL_APP_H__
