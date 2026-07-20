#include "envelope_channel.hpp"

#include <QIODevice>
#include <QLocalSocket>
#include <QUuid>

namespace mps::ipc
{
	EnvelopeChannel::EnvelopeChannel(QIODevice* device, QObject* parent)
		: QObject(parent)
		, device_(device)
	{
		Q_ASSERT(device_);
		connect(device_, &QIODevice::readyRead, this, &EnvelopeChannel::onReadyRead);
		if (auto* ls = qobject_cast<QLocalSocket*>(device_))
		{
			connect(ls, &QLocalSocket::disconnected, this, &EnvelopeChannel::disconnected);
		}
	}

	void EnvelopeChannel::setHandler(Handler handler)
	{
		handler_ = std::move(handler);
	}

	bool EnvelopeChannel::send(const shell::ipc::v1::Envelope& env)
	{
		if (!device_ || !device_->isOpen())
		{
			return false;
		}
		std::string payload;
		if (!env.SerializeToString(&payload))
		{
			return false;
		}
		const auto frame = encodeFrame(payload);
		if (frame.empty())
		{
			return false;
		}
		const auto n = device_->write(reinterpret_cast<const char*>(frame.data()), static_cast<qint64>(frame.size()));
		return n == static_cast<qint64>(frame.size());
	}

	void EnvelopeChannel::onReadyRead()
	{
		if (!device_)
		{
			return;
		}
		const QByteArray chunk = device_->readAll();
		if (chunk.isEmpty())
		{
			return;
		}
		decoder_.append(reinterpret_cast<const std::uint8_t*>(chunk.constData()),
						static_cast<std::size_t>(chunk.size()));

		for (;;)
		{
			std::string payload;
			const auto st = decoder_.tryPop(payload);
			if (st == FrameError::Incomplete)
			{
				break;
			}
			if (st == FrameError::PayloadTooLarge)
			{
				decoder_.reset();
				break;
			}
			shell::ipc::v1::Envelope env;
			if (!env.ParseFromString(payload))
			{
				continue;
			}
			if (handler_)
			{
				handler_(std::move(env));
			}
		}
	}

	std::string newCorrelationId()
	{
		return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
	}
} // namespace mps::ipc
