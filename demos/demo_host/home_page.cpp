#include "home_page.hpp"

#include "shell_app.hpp"
#include "shell_window.hpp"
#include "theme_origin.hpp"
#include "theme_scheme.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace mps::demo_host
{
	HomePage::HomePage(mps::host::ShellApp* app, mps::host::ShellWindow* shell, QWidget* parent)
		: QWidget(parent)
	{
		auto* lay = new QVBoxLayout(this);
		auto* createBtn = new QPushButton(QStringLiteral("Create Client"), this);
		createBtn->setFixedSize(160, 40);
		auto* lightBtn = new QPushButton(QStringLiteral("Light"), this);
		auto* darkBtn = new QPushButton(QStringLiteral("Dark"), this);
		lightBtn->setFixedSize(80, 32);
		darkBtn->setFixedSize(80, 32);
		auto* themeRow = new QHBoxLayout();
		themeRow->setSpacing(12);
		themeRow->addStretch();
		themeRow->addWidget(lightBtn);
		themeRow->addWidget(darkBtn);
		themeRow->addStretch();
		lay->addStretch();
		lay->addWidget(createBtn, 0, Qt::AlignCenter);
		lay->addSpacing(16);
		lay->addLayout(themeRow);
		lay->addStretch();

		connect(createBtn, &QPushButton::clicked, this,
				[app, shell]()
				{
					if (app && shell)
					{
						app->createClientOn(shell);
					}
				});
		connect(lightBtn, &QPushButton::clicked, this,
				[app]()
				{
					if (app)
					{
						app->setScheme(mps::theme::Scheme::Light, mps::host::ThemeOrigin::HostUi);
					}
				});
		connect(darkBtn, &QPushButton::clicked, this,
				[app]()
				{
					if (app)
					{
						app->setScheme(mps::theme::Scheme::Dark, mps::host::ThemeOrigin::HostUi);
					}
				});
	}
} // namespace mps::demo_host
