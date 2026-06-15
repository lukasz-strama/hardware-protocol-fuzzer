#ifndef FUZZ_RESULTS_MODEL_H
#define FUZZ_RESULTS_MODEL_H

#include <QAbstractTableModel>
#include <QQueue>
#include <QVector>
#include <QString>

/**
 * @brief Result of a single fuzz stimulus.
 */
struct FuzzResult {
    int      stimulusId   = 0;
    QString  sentData;               ///< Hex string of bytes sent
    QString  responseEvent;          ///< e.g. "BYTE", "NACK", "TIMEOUT", "OVERFLOW"
    QString  responseData;           ///< Hex string of response bytes
    QString  verdict;                ///< "OK", "NACK", "TIMEOUT", "ERROR"
    quint32  responseTimestampUs = 0;
    quint32  traceSeq    = 0;
};

/**
 * @brief Table model for fuzz results with stimulus↔response correlation.
 *
 * Columns: Stimulus ID | Sent Data | Response | Response Data | Verdict
 *
 * The model maintains two sets of pending records:
 *  - stimuli that have been sent (via FUZZ_TX) but not yet matched,
 *  - responses (via TRACE_DECODED) that arrived after a fuzz session.
 *
 * Correlation is done by matching trace_seq from FUZZ_TX with subsequent
 * TRACE_DECODED events.
 */
class FuzzResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColStimulusId = 0,
        ColSentData,
        ColResponseEvent,
        ColResponseData,
        ColVerdict,
        ColumnCount
    };

    explicit FuzzResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief Record that a stimulus was sent (from FUZZ_TX frame).
     */
    void addStimulusSent(int stimulusId, quint32 traceSeq,
                         const QString &dataHex);

    /**
     * @brief Record a response from the target (from TRACE_DECODED frame).
     *
     * Tries to match with the most recent unmatched stimulus.
     */
    void addResponse(quint32 traceSeq, const QString &event,
                     const QString &dataHex, quint32 timestampUs);

    /**
     * @brief Mark all unmatched stimuli as TIMEOUT.
     */
    void finaliseTimeouts();

    /**
     * @brief Clear all results.
     */
    void clear();

    /* ── Statistics ─────────────────────────────────────────────── */

    int totalCount() const;
    int okCount() const;
    int nackCount() const;
    int timeoutCount() const;
    int errorCount() const;

private:
    struct PendingResponse {
        QString event;
        QString dataHex;
        quint32 timestampUs = 0;
    };

    struct PendingStimulus {
        int stimulusId = 0;
        quint32 traceSeq = 0;
        int rowIndex = -1;
        QString dataHex;
    };

    QVector<FuzzResult> results_;
    QQueue<PendingStimulus> pendingStimuli_;
    QQueue<PendingResponse> pendingResponses_;
};

#endif // FUZZ_RESULTS_MODEL_H
