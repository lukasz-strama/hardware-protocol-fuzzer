#include "fuzz_orchestrator.h"
#include "fuzz_results_model.h"
#include "backend_bridge.h"

#include <QElapsedTimer>
#include <QCoreApplication>
#include <QThread>

extern "C" {
#include "pico_host.h"
}

FuzzOrchestrator::FuzzOrchestrator(PicoBackend *backend,
                                   FuzzResultsModel *model,
                                   QObject *parent)
    : QObject(parent)
    , backend_(backend)
    , model_(model)
    , pumpTimer_(new QTimer(this))
{
    connect(pumpTimer_, &QTimer::timeout, this, &FuzzOrchestrator::onPumpTick);
    connect(backend_, &PicoBackend::frameReceived,
            this, &FuzzOrchestrator::onFrameReceived);
}

void FuzzOrchestrator::startSession(const QVector<CorpusEntry> &corpus,
                                     uint8_t busType,
                                     uint8_t selectionMode,
                                     uint8_t repeatMode,
                                     uint8_t policyFlags,
                                     uint32_t budgetMs)
{
    if (running_) {
        emit logMessage("[fuzz] Session already running");
        return;
    }
    if (!backend_->isOpen()) {
        emit logMessage("[fuzz] Backend not connected");
        return;
    }
    if (corpus.isEmpty()) {
        emit logMessage("[fuzz] Corpus is empty");
        return;
    }

    model_->clear();
    budgetMs_ = budgetMs;
    currentBusType_ = busType;

    /* ── 1. Send SET_FUZZ_POLICY ─────────────────────────────── */
    hw_protocol_set_fuzz_policy_t policy = {};
    policy.time_budget_ms = budgetMs;
    policy.pending_bytes  = 4096;
    policy.policy_flags   = policyFlags;
    policy.selection_mode = selectionMode;
    policy.repeat_mode    = repeatMode;
    policy.max_pending    = static_cast<uint8_t>(qMin(corpus.size(), 32));

    if (backend_->sendSetFuzzPolicy(policy) != PICO_OK) {
        emit logMessage("[fuzz] SET_FUZZ_POLICY failed");
        return;
    }
    /* Pump to process firmware STATUS response for policy acceptance */
    backend_->pump();
    emit logMessage(QString("[fuzz] SET_FUZZ_POLICY budget=%1ms flags=0x%2 selection=%3 repeat=%4")
        .arg(budgetMs).arg(policyFlags, 2, 16, QLatin1Char('0'))
        .arg(selectionMode).arg(repeatMode));

    /* ── 2. Queue stimuli ────────────────────────────────────── */
    int queued = 0;
    for (const auto &entry : corpus) {
        if (queued >= 32) break;  /* firmware limit */

        uint8_t flags = HW_PROTOCOL_STIMULUS_INLINE_PAYLOAD;
        if (&entry == &corpus.last()) {
            flags |= HW_PROTOCOL_STIMULUS_LAST_IN_SEQUENCE;
        }

        pico_result_t r = backend_->sendQueueStimulus(
            static_cast<uint32_t>(entry.id > 0 ? entry.id : nextStimulusId_),
            reinterpret_cast<const uint8_t *>(entry.data.constData()),
            static_cast<uint16_t>(entry.data.size()),
            flags,
            0  /* stimulus_kind = raw_bytes */
        );

        if (r != PICO_OK) {
            emit logMessage(QString("[fuzz] QUEUE_STIMULUS #%1 failed").arg(queued + 1));
            break;
        }
        ++queued;
        ++nextStimulusId_;
    }
    emit logMessage(QString("[fuzz] Queued %1 stimuli").arg(queued));

    /* ── 3. Wait for firmware to acknowledge ─────────────────── */
    backend_->pump();

    /* ── 4. START_FUZZ ───────────────────────────────────────── */
    if (backend_->sendStartFuzz() != PICO_OK) {
        emit logMessage("[fuzz] START_FUZZ failed");
        return;
    }

    running_ = true;
    pumpTimer_->start(20);  /* pump every 20 ms */
    emit sessionStarted();
    emit logMessage("[fuzz] Session started");
}

void FuzzOrchestrator::stopSession()
{
    if (!running_) return;

    pumpTimer_->stop();

    /* Send STOP — may fail if firmware already auto-stopped (queue drained
       or time budget expired), which is fine. */
    pico_result_t r = backend_->sendStop();
    if (r == PICO_OK) {
        backend_->waitForState(HW_PROTOCOL_STATE_ARMED, HW_PROTOCOL_STOP_TIMEOUT_MS);
    } else {
        emit logMessage("[fuzz] STOP not sent (firmware may have auto-stopped)");
    }

     /* Give late TRACE_DECODED frames a wider window to arrive before
         finalising pending stimuli as TIMEOUT.
         The firmware can drain the queue and return to ARMED before the
         DUT reply has fully propagated through capture + USB CDC. */
     for (int i = 0; i < 40; ++i) {
        backend_->pump();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }

    running_ = false;

    model_->finaliseTimeouts();

    emit sessionStopped(model_->totalCount(),
                        model_->okCount(),
                        model_->nackCount(),
                        model_->timeoutCount());
    emit logMessage(QString("[fuzz] Session stopped — total=%1 ok=%2 nack=%3 timeout=%4")
        .arg(model_->totalCount())
        .arg(model_->okCount())
        .arg(model_->nackCount())
        .arg(model_->timeoutCount()));
}

void FuzzOrchestrator::onPumpTick()
{
    if (!running_ || !backend_->isOpen()) {
        stopSession();
        return;
    }

    backend_->pump();

    /* Check if firmware auto-stopped (queue drained or time budget) */
    hw_protocol_session_state_t st = backend_->state();
    if (st == HW_PROTOCOL_STATE_ARMED || st == HW_PROTOCOL_STATE_FAULT) {
        emit logMessage("[fuzz] Firmware reports session ended");
        stopSession();
    }
}

void FuzzOrchestrator::onFrameReceived(int type, quint16 /*sessionId*/,
                                        quint32 sequence,
                                        const QByteArray &payload)
{
    if (!running_) return;

    const auto msgType = static_cast<msg_type_t>(type);

    if (msgType == MSG_FUZZ_TX && payload.size() >= 12) {
        /* Decode FUZZ_TX payload (explicit byte decoding) */
        quint32 stimId = static_cast<quint32>(static_cast<unsigned char>(payload.at(0)))
                       | (static_cast<quint32>(static_cast<unsigned char>(payload.at(1))) << 8)
                       | (static_cast<quint32>(static_cast<unsigned char>(payload.at(2))) << 16)
                       | (static_cast<quint32>(static_cast<unsigned char>(payload.at(3))) << 24);
        quint32 traceSeq = static_cast<quint32>(static_cast<unsigned char>(payload.at(4)))
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(5))) << 8)
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(6))) << 16)
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(7))) << 24);
        quint16 dataLen = static_cast<quint16>(static_cast<unsigned char>(payload.at(10)))
                        | (static_cast<quint16>(static_cast<unsigned char>(payload.at(11))) << 8);

        QString dataHex;
        int available = qMin<int>(dataLen, payload.size() - 12);
        for (int i = 0; i < available; ++i) {
            dataHex += QString("%1 ")
                .arg(static_cast<unsigned char>(payload.at(12 + i)), 2, 16, QLatin1Char('0'));
        }

        model_->addStimulusSent(static_cast<int>(stimId), traceSeq,
                                dataHex.trimmed().toUpper());

    } else if (msgType == MSG_TRACE_DECODED && payload.size() >= 12) {
        quint32 traceSeq = static_cast<quint32>(static_cast<unsigned char>(payload.at(0)))
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(1))) << 8)
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(2))) << 16)
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(3))) << 24);
        quint32 tsUs     = static_cast<quint32>(static_cast<unsigned char>(payload.at(4)))
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(5))) << 8)
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(6))) << 16)
                         | (static_cast<quint32>(static_cast<unsigned char>(payload.at(7))) << 24);
        quint16 dataLen  = static_cast<quint16>(static_cast<unsigned char>(payload.at(8)))
                         | (static_cast<quint16>(static_cast<unsigned char>(payload.at(9))) << 8);
        uint8_t eventType = static_cast<uint8_t>(payload.at(11));

        QString eventName = "BYTE";
        switch (eventType) {
            case HW_PROTOCOL_EVENT_ACK:      eventName = "ACK";      break;
            case HW_PROTOCOL_EVENT_NACK:     eventName = "NACK";     break;
            case HW_PROTOCOL_EVENT_OVERFLOW: eventName = "OVERFLOW"; break;
            case HW_PROTOCOL_EVENT_START:    eventName = "START";    break;
            case HW_PROTOCOL_EVENT_STOP:     eventName = "STOP";     break;
            case HW_PROTOCOL_EVENT_BREAK:    eventName = "BREAK";    break;
            default: break;
        }

        QString dataHex;
        int available = qMin<int>(dataLen, payload.size() - 12);
        for (int i = 0; i < available; ++i) {
            dataHex += QString("%1 ")
                .arg(static_cast<unsigned char>(payload.at(12 + i)), 2, 16, QLatin1Char('0'));
        }

        model_->addResponse(traceSeq, eventName,
                             dataHex.trimmed().toUpper(), tsUs);
    }
}
