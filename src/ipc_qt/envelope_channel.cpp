#include "envelope_channel.hpp"

#include "envelope_codec.hpp"

#include <QIODevice>
#include <QLocalSocket>
#include <QUuid>

namespace mps::ipc
{
	EnvelopeChannel::EnvelopeChannel(QIODevice* device, QObject* parent)
		: QObject(parent)
		, m_device(device)
	{
		Q_ASSERT(m_device);
		if (auto* ls = qobject_cast<QLocalSocket*>(m_device))
		{
			connect(ls, &QLocalSocket::disconnected, this, &EnvelopeChannel::disconnected);
		}
		// Do not connect readyRead until setHandler: otherwise a Hello that arrives in the
		// race window is parsed with a null handler and dropped (bytes already consumed).
	}

	void EnvelopeChannel::setHandler(Handler handler)
	{
		m_handler = std::move(handler);
		if (m_device && !m_readyReadHooked)
		{
			connect(m_device, &QIODevice::readyRead, this, &EnvelopeChannel::onReadyRead);
			m_readyReadHooked = true;
		}
		// Data may already be buffered if the peer wrote before the handler was set.
		if (m_device && m_device->bytesAvailable() > 0)
		{
			onReadyRead();
		}
	}

	bool EnvelopeChannel::send(const shell::ipc::v1::Envelope& env)
	{
		if (!m_device || !m_device->isOpen())
		{
			return false;
		}
		std::string payload;
		if (!serializeEnvelope(env, &payload))
		{
			return false;
		}
		const auto frame = encodeFrame(payload);
		if (frame.empty())
		{
			return false;
		}
		const auto n = m_device->write(reinterpret_cast<const char*>(frame.data()), static_cast<qint64>(frame.size()));
		if (auto* ls = qobject_cast<QLocalSocket*>(m_device))
		{
			ls->flush();
		}
		return n == static_cast<qint64>(frame.size());
	}

	bool EnvelopeChannel::send(const EnvelopePtr& env)
	{
		return env && send(*env);
	}

	void EnvelopeChannel::onReadyRead()
	{
		if (!m_device)
		{
			return;
		}
		const QByteArray chunk = m_device->readAll();
		if (chunk.isEmpty())
		{
			return;
		}
		m_decoder.append(reinterpret_cast<const std::uint8_t*>(chunk.constData()), static_cast<std::size_t>(chunk.size()));

		for (;;)
		{
			std::string payload;
			const auto st = m_decoder.tryPop(payload);
			if (st == FrameError::Incomplete)
			{
				break;
			}
			if (st == FrameError::PayloadTooLarge)
			{
				m_decoder.reset();
				break;
			}
			auto env = parseEnvelope(payload);
			if (!env)
			{
				continue;
			}
			if (m_handler)
			{
				m_handler(std::move(env));
			}
		}
	}

	std::string newCorrelationId()
	{
		return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
	}
} // namespace mps::ipc
