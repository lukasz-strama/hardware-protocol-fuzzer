#ifndef FUZZ_ORCHESTRATOR_H
#define FUZZ_ORCHESTRATOR_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

extern "C" {
#include "pico_host.h"
}

class PicoBackend;
class FuzzResultsModel;

/**
 * @brief One entry in the local corpus managed by the desktop.
 */
struct CorpusEntry {
    int      id = 0;
    QByteArray data;
    QString  label;          ///< Human-readable description
};

/**
 * @brief Orchestrates a complete fuzz session.
 *
 * Flow:
 *  1. SET_FUZZ_POLICY with user-chosen parameters
 *  2. Batch QUEUE_STIMULUS for each corpus entry
 *  3. START_FUZZ
 *  4. Poll loop: pump() collecting FUZZ_TX + TRACE_DECODED
 *  5. Auto-stop after timeout or queue drained
 *  6. Finalise results (mark remaining as TIMEOUT)
 */
class FuzzOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit FuzzOrchestrator(PicoBackend *backend,
                              FuzzResultsModel *model,
                              QObject *parent = nullptr);

    /**
     * @brief Start a fuzz session with the given corpus and policy.
     *
     * @param corpus     List of stimuli to send.
     * @param busType    0 = I2C, 1 = UART (maps to hw_protocol_bus_type_t).
     * @param selectionMode  0 = sequential, 1 = random, 2 = corpus-guided.
     * @param repeatMode     0 = once, 1 = repeat, 2 = mutate-once.
     * @param policyFlags    Bitmask of FUZZ_POLICY_* flags.
     * @param budgetMs       Time budget in milliseconds (0 = no limit).
     */
    void startSession(const QVector<CorpusEntry> &corpus,
                      uint8_t busType,
                      uint8_t selectionMode,
                      uint8_t repeatMode,
                      uint8_t policyFlags,
                      uint32_t budgetMs);

    /**
     * @brief Stop the current fuzz session (sends STOP to firmware).
     */
    void stopSession();

    bool isRunning() const { return running_; }

signals:
    void sessionStarted();
    void sessionStopped(int totalStimuli, int okCount, int nackCount, int timeoutCount);
    void logMessage(const QString &msg);

private slots:
    void onPumpTick();
    void onFrameReceived(int type, quint16 sessionId, quint32 sequence,
                         const QByteArray &payload);

private:
    PicoBackend      *backend_;
    FuzzResultsModel *model_;
    QTimer           *pumpTimer_;
    bool              running_ = false;
    uint32_t          budgetMs_ = 0;
    uint8_t           currentBusType_ = 1;  // 0=I2C, 1=UART
    int               nextStimulusId_ = 1;
};

#endif // FUZZ_ORCHESTRATOR_H
