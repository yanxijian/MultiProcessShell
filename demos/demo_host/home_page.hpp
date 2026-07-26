#ifndef __MPS_DEMO_HOST_HOME_PAGE_H__
#define __MPS_DEMO_HOST_HOME_PAGE_H__

#include <QWidget>

namespace mps::host
{
	class ShellApp;
	class ShellWindow;
} // namespace mps::host

namespace mps::demo_host
{
	/// Demo-only Home client area: Create Client + Light/Dark (not part of mps_host).
	class HomePage final : public QWidget
	{
		Q_OBJECT
	public:
		HomePage(mps::host::ShellApp* app, mps::host::ShellWindow* shell, QWidget* parent = nullptr);
	};
} // namespace mps::demo_host

#endif // __MPS_DEMO_HOST_HOME_PAGE_H__
