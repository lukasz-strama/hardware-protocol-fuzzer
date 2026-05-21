#include "backend_bridge.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

extern "C" {
#include <string.h>
}

PicoBackend::PicoBackend(QObject *parent)
    : QObject(parent)
{
    memset(&session_, 0, sizeof(session_));
    session_.fd = -1;
    session_.state = HW_PROTOCOL_STATE_DETACHED;
    session_.callback_user_data = this;
    session_.on_frame = &PicoBackend::onFrameThunk;
    lastState_ = session_.state;
    supportedModes_ = 0;
    rxOverruns_ = 0;
    txUnderruns_ = 0;
    queuedStimuli_ = 0;
}

PicoBackend::~PicoBackend()
{
    closePort();
}

bool PicoBackend::openPort(const QString &port, int baudRate, QString *errorMessage)
{
    closePort();

    QByteArray portBytes = port.toLocal8Bit();
    pico_result_t result = transport_open(&session_, portBytes.constData(), baudRate);
    if (result != PICO_OK) {
        if (errorMessage) {
            *errorMessage = resultToString(result);
        }
        emitError(result, QString("[backend] Failed to open %1").arg(port));
        return false;
    }

    session_.callback_user_data = this;
    session_.on_frame = &PicoBackend::onFrameThunk;
    lastState_ = session_.state;
    supportedModes_ = 0;
    rxOverruns_ = 0;
    txUnderruns_ = 0;
    queuedStimuli_ = 0;
    emit backendUpdated();
    return true;
}

void PicoBackend::closePort()
{
    if (!isOpen()) {
        return;
    }

    transport_close(&session_);
    session_.callback_user_data = this;
    session_.on_frame = &PicoBackend::onFrameThunk;
    session_.state = HW_PROTOCOL_STATE_DETACHED;
    lastState_ = session_.state;
    emit backendUpdated();
}

bool PicoBackend::isOpen() const
{
    return session_.fd >= 0;
}

pico_result_t PicoBackend::sendHello()
{
    pico_result_t result = session_hello(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] HELLO failed");
    }
    return result;
}

pico_result_t PicoBackend::sendGetCaps()
{
    pico_result_t result = session_get_caps(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] GET_CAPS failed");
    }
    return result;
}

pico_result_t PicoBackend::sendGetStatus()
{
    pico_result_t result = session_get_status(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] GET_STATUS failed");
    }
    return result;
}

pico_result_t PicoBackend::sendSetBus(const hw_protocol_set_bus_t &bus)
{
    pico_result_t result = session_set_bus(&session_, &bus);
    if (result != PICO_OK) {
        emitError(result, "[backend] SET_BUS failed");
    }
    return result;
}

pico_result_t PicoBackend::sendSetTarget(const hw_protocol_set_target_t &target)
{
    pico_result_t result = session_set_target(&session_, &target);
    if (result != PICO_OK) {
        emitError(result, "[backend] SET_TARGET failed");
    }
    return result;
}

pico_result_t PicoBackend::sendArm()
{
    pico_result_t result = session_arm(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] ARM failed");
    }
    return result;
}

pico_result_t PicoBackend::sendStartCapture()
{
    pico_result_t result = session_start_capture(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] START_CAPTURE failed");
    }
    return result;
}

pico_result_t PicoBackend::sendStop()
{
    pico_result_t result = session_stop(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] STOP failed");
    }
    return result;
}

pico_result_t PicoBackend::sendDisarm()
{
    pico_result_t result = session_disarm(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] DISARM failed");
    }
    return result;
}

pico_result_t PicoBackend::sendSetFuzzPolicy(const hw_protocol_set_fuzz_policy_t &policy)
{
    pico_result_t result = session_set_fuzz_policy(&session_, &policy);
    if (result != PICO_OK) {
        emitError(result, "[backend] SET_FUZZ_POLICY failed");
    }
    return result;
}

pico_result_t PicoBackend::sendQueueStimulus(uint32_t stimulusId,
                                             const uint8_t *data,
                                             uint16_t dataLen,
                                             uint8_t flags,
                                             uint8_t kind)
{
    pico_result_t result = session_queue_stimulus(&session_, stimulusId, data, dataLen, flags, kind);
    if (result != PICO_OK) {
        emitError(result, "[backend] QUEUE_STIMULUS failed");
    }
    return result;
}

pico_result_t PicoBackend::sendStartFuzz()
{
    pico_result_t result = session_start_fuzz(&session_);
    if (result != PICO_OK) {
        emitError(result, "[backend] START_FUZZ failed");
    }
    return result;
}

pico_result_t PicoBackend::pump()
{
    if (!isOpen()) {
        return PICO_ERR_TRANSPORT;
    }

    pico_result_t result = session_pump(&session_);
    if (result == PICO_ERR_OVERFLOW) {
        emit logMessage("[backend] RX buffer overflow while pumping session");
        emit backendUpdated();
        return PICO_OK;
    }

    if (result != PICO_OK) {
        emitError(result, "[backend] Pump failed");
    }

    emit backendUpdated();
    return result;
}

bool PicoBackend::waitForState(hw_protocol_session_state_t expectedState, int timeoutMs)
{
    if (session_.state == expectedState) {
        return true;
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        pico_result_t result = pump();
        if (result == PICO_ERR_TRANSPORT || result == PICO_ERR_PROTOCOL || result == PICO_ERR_DEVICE) {
            return false;
        }

        if (session_.state == expectedState) {
            return true;
        }
        if (session_.state == HW_PROTOCOL_STATE_FAULT) {
            return false;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }

    emit logMessage(QString("[backend] Timeout waiting for %1").arg(state_name(expectedState)));
    return false;
}

hw_protocol_session_state_t PicoBackend::state() const
{
    return session_.state;
}

uint16_t PicoBackend::sessionId() const
{
    return session_.session_id;
}

uint8_t PicoBackend::supportedModes() const
{
    return supportedModes_;
}

uint32_t PicoBackend::rxOverruns() const
{
    return rxOverruns_;
}

uint32_t PicoBackend::txUnderruns() const
{
    return txUnderruns_;
}

uint8_t PicoBackend::queuedStimuli() const
{
    return queuedStimuli_;
}

void PicoBackend::onFrameThunk(void *userData,
                               const hw_protocol_frame_header_t *hdr,
                               const uint8_t *payload,
                               size_t payloadLen)
{
    if (!userData) {
        return;
    }

    auto *backend = static_cast<PicoBackend *>(userData);
    backend->onFrame(hdr, payload, payloadLen);
}

void PicoBackend::onFrame(const hw_protocol_frame_header_t *hdr,
                          const uint8_t *payload,
                          size_t payloadLen)
{
    QByteArray payloadBytes;
    if (payloadLen > 0 && payload) {
        payloadBytes = QByteArray(reinterpret_cast<const char *>(payload), static_cast<int>(payloadLen));
    }

    if (hdr->type == MSG_CAPS_RESPONSE && payloadLen >= 16) {
        supportedModes_ = payload[9];
    } else if (hdr->type == MSG_STATUS && payloadLen >= 20) {
        rxOverruns_ = (uint32_t)(payload[0] | ((uint32_t)payload[1] << 8)
                     | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24));
        txUnderruns_ = (uint32_t)(payload[4] | ((uint32_t)payload[5] << 8)
                     | ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24));
        queuedStimuli_ = payload[18];
    }

    emit frameReceived(hdr->type, hdr->session_id, hdr->sequence, payloadBytes);

    if (session_.state != lastState_) {
        lastState_ = session_.state;
    }

    emit backendUpdated();
}

void PicoBackend::emitError(pico_result_t result, const QString &prefix)
{
    if (result == PICO_OK) {
        return;
    }

    emit logMessage(QString("%1 (%2)").arg(prefix, resultToString(result)));
}

QString PicoBackend::resultToString(pico_result_t result) const
{
    switch (result) {
        case PICO_OK:           return "OK";
        case PICO_ERR_TRANSPORT:return "transport error";
        case PICO_ERR_TIMEOUT:   return "timeout";
        case PICO_ERR_PROTOCOL:  return "protocol error";
        case PICO_ERR_STATE:     return "invalid state";
        case PICO_ERR_DEVICE:    return "device error";
        case PICO_ERR_OVERFLOW:  return "overflow";
        default:                 return "unknown error";
    }
}
