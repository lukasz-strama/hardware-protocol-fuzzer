#ifndef BACKEND_BRIDGE_H
#define BACKEND_BRIDGE_H

#include <QObject>
#include <QByteArray>
#include <QString>

extern "C" {
#include "pico_host.h"
}

class PicoBackend : public QObject
{
    Q_OBJECT

public:
    explicit PicoBackend(QObject *parent = nullptr);
    ~PicoBackend() override;

    bool openPort(const QString &port, int baudRate, QString *errorMessage = nullptr);
    void closePort();
    bool isOpen() const;

    pico_result_t sendHello();
    pico_result_t sendGetCaps();
    pico_result_t sendGetStatus();
    pico_result_t sendSetBus(const hw_protocol_set_bus_t &bus);
    pico_result_t sendSetTarget(const hw_protocol_set_target_t &target);
    pico_result_t sendArm();
    pico_result_t sendStartCapture();
    pico_result_t sendStop();
    pico_result_t sendDisarm();
    pico_result_t sendSetFuzzPolicy(const hw_protocol_set_fuzz_policy_t &policy);
    pico_result_t sendQueueStimulus(uint32_t stimulusId,
                                    const uint8_t *data,
                                    uint16_t dataLen,
                                    uint8_t flags,
                                    uint8_t kind);
    pico_result_t sendStartFuzz();

    pico_result_t pump();
    bool waitForState(hw_protocol_session_state_t expectedState, int timeoutMs);

    hw_protocol_session_state_t state() const;
    uint16_t sessionId() const;
    uint8_t supportedModes() const;
    uint32_t rxOverruns() const;
    uint32_t txUnderruns() const;
    uint8_t queuedStimuli() const;

signals:
    void frameReceived(int type, quint16 sessionId, quint32 sequence, QByteArray payload);
    void backendUpdated();
    void logMessage(const QString &message);

private:
    static void onFrameThunk(void *userData,
                             const hw_protocol_frame_header_t *hdr,
                             const uint8_t *payload,
                             size_t payloadLen);

    void onFrame(const hw_protocol_frame_header_t *hdr,
                 const uint8_t *payload,
                 size_t payloadLen);
    void emitError(pico_result_t result, const QString &prefix);
    QString resultToString(pico_result_t result) const;

    pico_session_t session_;
    hw_protocol_session_state_t lastState_;
    uint8_t supportedModes_;
    uint32_t rxOverruns_;
    uint32_t txUnderruns_;
    uint8_t queuedStimuli_;
};

#endif
