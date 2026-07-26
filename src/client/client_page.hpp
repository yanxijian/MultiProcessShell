#ifndef __MPS_CLIENT_CLIENT_PAGE_H__
#define __MPS_CLIENT_CLIENT_PAGE_H__

#include "theme_scheme.hpp"

#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

namespace mps::client
{
	/// Abstract page owned by ClientApp (NOT a QWidget subclass — composition).
	class ClientPage
	{
	public:
		virtual ~ClientPage() = default;
		[[nodiscard]] virtual QWidget* widget() = 0;
		[[nodiscard]] virtual qint64 tabId() const = 0;
		virtual void realizeChrome()
		{
		}
		virtual void syncAfterEmbed()
		{
		}
		virtual void applyTheme(mps::theme::Scheme)
		{
		}

		std::function<void()> onRequestNewWindow;
		std::function<void(mps::theme::Scheme)> onRequestTheme;
	};

	using PageFactory = std::function<std::unique_ptr<ClientPage>(qint64 tabId, const QString& title)>;
} // namespace mps::client

#endif // __MPS_CLIENT_CLIENT_PAGE_H__
