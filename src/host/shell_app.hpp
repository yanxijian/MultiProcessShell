#ifndef __MPS_HOST_SHELL_APP_H__
#define __MPS_HOST_SHELL_APP_H__

#include "client_session.hpp"
#include "shell_window.hpp"
#include "tear_out_preview.hpp"
#include "theme_origin.hpp"
#include "theme_scheme.hpp"

#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>

#include <functional>
#include <memory>
#include <mps/mps_host_export.hpp>
#include <vector>

namespace mps::host
{
	class MPS_HOST_EXPORT ShellApp final : public QObject
	{
		Q_OBJECT
	public:
		using HomeContentFactory = std::function<QWidget*(ShellWindow*)>;

		/// endpointPrefix forms the pipe name "{prefix}-{token}". Default "mps" (Demo passes "mps-demo").
		explicit ShellApp(QString clientExe, QString endpointPrefix = QStringLiteral("mps"), QObject* parent = nullptr);
		~ShellApp() override;
		[[nodiscard]] ShellWindow* createShell(QPoint pos = {}, QSize size = {}, bool showNow = true);
		/// Launch the default Client exe (constructor path). Demo-compatible.
		void createClientOn(ShellWindow* shell);
		/// Launch or reuse a registered Client by appName (Volition: one process per kind).
		void createClientOn(ShellWindow* shell, const QString& appName);
		/// Register appName → client executable (absolute or beside Host). Overrides prior entry.
		void registerClientLauncher(const QString& appName, const QString& exePath);
		/// First Host tab id for a live session of appName, or 0.
		[[nodiscard]] qint64 findTabIdForApp(const QString& appName) const;
		/// Host → Client Invoke on a ContentView tab (requires live session owning tabId).
		void invokeOnTab(qint64 tabId, const QString& method, const QByteArray& params);
		void closeTab(qint64 tabId);
		void activateTab(ShellWindow* shell, qint64 tabId);
		void tearOutTab(ShellWindow* source, qint64 tabId, QRect suggestedGeometry);
		void mergeTab(qint64 tabId, ShellWindow* target, int insertIndex = -1);
		void closeShell(ShellWindow* shell);
		void clearAllDropIndicators();
		void beginTabDrag(ShellWindow* source, qint64 tabId, QPoint localHotSpot = {});
		void noteTabDragDropHandled();
		[[nodiscard]] bool consumeDragCancelled();
		[[nodiscard]] bool isDragAutoMerged() const;
		[[nodiscard]] bool isAutoMergeAnimating() const;
		[[nodiscard]] bool shouldSuppressTearOutAt(QPoint globalPos) const;
		[[nodiscard]] bool isReleaseOverWindowButtons(QPoint globalPos) const;
		[[nodiscard]] QRect tearOutPreviewGeometry() const;
		void endTabDrag(bool tearOrMerge);
		[[nodiscard]] ShellWindow* shellForTab(qint64 tabId) const;
		[[nodiscard]] ShellWindow* shellAtGlobal(QPoint globalPos) const;
		[[nodiscard]] ShellWindow* shellFromStripDropTarget(QObject* watched) const;
		[[nodiscard]] ShellWindow* tabDropZoneShellAtGlobal(QPoint globalPos) const;
		void destroyShellIfEmpty(ShellWindow* shell);

		/// Per-shell Home client-area factory (Demo installs Create Client / theme UI).
		void setHomeContentFactory(HomeContentFactory factory)
		{
			m_homeContentFactory = std::move(factory);
		}
		void setShellWindowTitle(QString title)
		{
			m_shellWindowTitle = std::move(title);
		}
		/// Client Invoke method that requests another ContentView in the same session.
		void setRequestNewContentViewMethod(QString method)
		{
			m_requestNewContentViewMethod = std::move(method);
		}
		[[nodiscard]] QString requestNewContentViewMethod() const
		{
			return m_requestNewContentViewMethod;
		}

		[[nodiscard]] mps::theme::Scheme scheme() const
		{
			return m_scheme;
		}
		/// Update appearance SSOT (Light/Dark). Broadcasts to clients when origin != Startup.
		void setScheme(mps::theme::Scheme scheme, ThemeOrigin origin);

	signals:
		void schemeChanged(mps::theme::Scheme scheme, ThemeOrigin origin);
		/// Emitted when a ContentView for a registered appName becomes ready (SubWindowAdded).
		void appContentViewReady(const QString& appName, qint64 tabId);

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override;

	private:
		void onNewConnection();
		void bindShell(ShellWindow* shell);
		void onSessionReady(ClientSession* session);
		void onSessionHelloOk(ClientSession* session);
		void onContentViewReady(ClientSession* session, qint64 tabId, QString title, quintptr wid);
		void onContentViewClosed(ClientSession* session, qint64 tabId);
		void onSessionDead(ClientSession* session);
		void onSessionUnhealthy(ClientSession* session);
		void onSessionHealthy(ClientSession* session);
		void terminateSession(ClientSession* session);
		void onThemeSetRequested(ClientSession* session, mps::theme::Scheme scheme);
		void pushThemeToSession(ClientSession* session);
		void broadcastTheme(mps::theme::Scheme scheme);
		void updateTabDragVisuals();
		void clearAllTabYieldPreviews();
		void pollEscapeCancel();
		void startGhostSnapBack();
		void finishGhostSnapBack();
		void flushCreatesDeferredDuringDrag();
		void tryCommitMagneticAutoMerge();
		void startAutoMergeAnimation(ShellWindow* source, ShellWindow* target, qint64 tabId, int insertIndex);
		void finishAutoMergeAnimation();
		void requestAbortOleDrag();
		void showDragDropSink(bool on);
		[[nodiscard]] bool isDragDropSink(QObject* watched) const;
		[[nodiscard]] bool shellStillAlive(ShellWindow* shell) const;
		[[nodiscard]] QString makeTitle(int instanceIndex, int contentIndex) const;
		[[nodiscard]] QString resolveClientExe(const QString& appName) const;
		void createClientOnWithExe(ShellWindow* shell, const QString& clientExe, const QString& appName = {});
		/// Request another ContentView on an existing live session (same Client process).
		void requestContentViewOnSession(ClientSession* session, ShellWindow* shell);

		struct DeferredCreate
		{
			ClientSession* session = nullptr;
			qint64 tabId = 0;
			QString title;
			ShellWindow* preferredShell = nullptr;
		};

		QString m_clientExe;
		QHash<QString, QString> m_clientLaunchers;
		/// appName → live session (Volition multi-kind: one process per kind).
		QHash<QString, ClientSession*> m_sessionsByAppName;
		QHash<ClientSession*, QString> m_sessionToAppName;
		QString m_endpoint;
		QString m_token;
		QString m_shellWindowTitle = QStringLiteral("Shell");
		QString m_requestNewContentViewMethod = QStringLiteral("demo.request_new_window");
		HomeContentFactory m_homeContentFactory;
		QLocalServer* m_server = nullptr;
		mps::theme::Scheme m_scheme = mps::theme::Scheme::Light;
		std::vector<std::unique_ptr<ShellWindow>> m_shells;
		std::vector<std::unique_ptr<ClientSession>> m_sessions;
		QHash<qint64, ShellWindow*> m_tabToShell;
		qint64 m_nextTabId = 1;
		int m_nextInstanceIndex = 1;
		QHash<int, int> m_nextContentIndex;
		QHash<ClientSession*, ShellWindow*> m_pendingFirstShell;
		std::vector<DeferredCreate> m_deferredCreatesDuringDrag;
		bool m_clientLaunchInFlight = false;

		TearOutPreview* m_tearOutPreview = nullptr;
		TabDragGhost* m_tabDragGhost = nullptr;
		QWidget* m_dragDropSink = nullptr;
		QTimer* m_dragVisualTimer = nullptr;
		QPropertyAnimation* m_ghostSnapAnim = nullptr;
		QParallelAnimationGroup* m_autoMergeAnim = nullptr;
		ShellWindow* m_dragSource = nullptr;
		QPointer<ShellWindow> m_pendingMergeTarget;
		QPointer<ShellWindow> m_pendingMergeSource;
		QPointer<ShellWindow> m_shellPendingDestroy;
		qint64 m_dragTabId = 0;
		qint64 m_dragResumeTabId = 0;
		qint64 m_pendingMergeTabId = 0;
		int m_pendingMergeIndex = -1;
		QPoint m_dragHotSpot{40, 20};
		QPoint m_tabGhostHotSpot{20, 16};
		QSize m_dragPreviewSize{720, 480};
		int m_dragTabWidth = 0;
		bool m_dragDropHandled = false;
		bool m_dragActive = false;
		bool m_dragCancelled = false;
		bool m_dragAutoMerged = false;
		bool m_autoMergeAnimActive = false;
		bool m_finishAutoMergeGuard = false;
		bool m_tearOutDetached = false;
		bool m_dragMoveWholeShell = false;
		bool m_ghostSnapBackActive = false;
		bool m_dragForbiddenCursor = false;
		QRect m_dragSourceSavedGeometry;
		QPoint m_dragWindowHotSpot;
	};
} // namespace mps::host

#endif // __MPS_HOST_SHELL_APP_H__
