#ifndef __MPS_IPC_QT_ENVELOPE_CHANNEL_H__
#define __MPS_IPC_QT_ENVELOPE_CHANNEL_H__

#include "frame.hpp"
#include "shell/ipc/v1/ipc.pb.h"

#include <QByteArray>
#include <QObject>

#include <functional>
#include <string>

class QIODevice;

namespace mps::ipc
{
	/// Bidirectional Envelope stream over a QIODevice (QLocalSocket / QLocalServer socket).
	class EnvelopeChannel : public QObject
	{
		Q_OBJECT
	public:
		using Handler = std::function<void(shell::ipc::v1::Envelope)>;

		explicit EnvelopeChannel(QIODevice* device, QObject* parent = nullptr);

		void setHandler(Handler handler);
		[[nodiscard]] bool send(const shell::ipc::v1::Envelope& env);
		[[nodiscard]] QIODevice* device() const
		{
			return m_device;
		}

	signals:
		void disconnected();

	private slots:
		void onReadyRead();

	private:
		QIODevice* m_device = nullptr;
		FrameDecoder m_decoder;
		Handler m_handler;
		QByteArray m_readBuf;
	};

	[[nodiscard]] std::string newCorrelationId();
} // namespace mps::ipc

#endif  // __MPS_IPC_QT_ENVELOPE_CHANNEL_H__
