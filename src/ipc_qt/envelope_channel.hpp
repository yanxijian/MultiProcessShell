#ifndef __MPS_IPC_QT_ENVELOPE_CHANNEL_H__
#define __MPS_IPC_QT_ENVELOPE_CHANNEL_H__

#include "envelope_codec.hpp"
#include "frame.hpp"
#include "shell/ipc/v1/ipc.pb.h"

#include <QByteArray>
#include <QObject>

#include <functional>
#include <mps/mps_ipc_qt_export.hpp>
#include <string>

class QIODevice;

namespace mps::ipc
{
	/// Bidirectional Envelope stream over a QIODevice (QLocalSocket / QLocalServer socket).
	class MPS_IPC_QT_EXPORT EnvelopeChannel : public QObject
	{
		Q_OBJECT
	public:
		/// Parsed envelopes use EnvelopePtr (allocated/freed in mps_ipc.dll).
		using Handler = std::function<void(EnvelopePtr)>;

		explicit EnvelopeChannel(QIODevice* device, QObject* parent = nullptr);

		void setHandler(Handler handler);
		[[nodiscard]] bool send(const shell::ipc::v1::Envelope& env);
		[[nodiscard]] bool send(const EnvelopePtr& env);
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
		bool m_readyReadHooked = false;
	};

	[[nodiscard]] std::string newCorrelationId();
} // namespace mps::ipc

#endif // __MPS_IPC_QT_ENVELOPE_CHANNEL_H__
