#include "fuzz_results_model.h"

#include <QColor>
#include <QFont>

FuzzResultsModel::FuzzResultsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int FuzzResultsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return results_.size();
}

int FuzzResultsModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant FuzzResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= results_.size())
        return {};

    const FuzzResult &r = results_.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColStimulusId:    return r.stimulusId;
        case ColSentData:      return r.sentData;
        case ColResponseEvent: return r.responseEvent;
        case ColResponseData:  return r.responseData;
        case ColVerdict:       return r.verdict;
        }
    }

    if (role == Qt::ForegroundRole && index.column() == ColVerdict) {
        if (r.verdict == "OK")      return QColor("#12805c");
        if (r.verdict == "NACK")    return QColor("#b42318");
        if (r.verdict == "TIMEOUT") return QColor("#b45309");
        if (r.verdict == "ERROR")   return QColor("#b42318");
        return QColor("#667085");
    }

    if (role == Qt::FontRole && index.column() == ColVerdict) {
        QFont font;
        font.setBold(true);
        return font;
    }

    if (role == Qt::FontRole && (index.column() == ColSentData || index.column() == ColResponseData)) {
        return QFont("Menlo", 11);
    }

    return {};
}

QVariant FuzzResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColStimulusId:    return "Stim ID";
    case ColSentData:      return "Sent Data";
    case ColResponseEvent: return "Response";
    case ColResponseData:  return "Resp Data";
    case ColVerdict:       return "Verdict";
    }
    return {};
}

void FuzzResultsModel::addStimulusSent(int stimulusId, quint32 traceSeq,
                                        const QString &dataHex)
{
    if (!pendingResponses_.isEmpty()) {
        PendingResponse pending = pendingResponses_.dequeue();

        FuzzResult r;
        r.stimulusId = stimulusId;
        r.traceSeq   = traceSeq;
        r.sentData   = dataHex;
        r.responseEvent       = pending.event;
        r.responseData        = pending.dataHex;
        r.responseTimestampUs = pending.timestampUs;
        r.verdict             = (pending.event == "NACK") ? "NACK"
                              : (pending.event == "OVERFLOW") ? "ERROR"
                              : "OK";

        beginInsertRows(QModelIndex(), results_.size(), results_.size());
        results_.append(r);
        endInsertRows();
        return;
    }

    const int rowIndex = results_.size();
    FuzzResult r;
    r.stimulusId = stimulusId;
    r.traceSeq   = traceSeq;
    r.sentData   = dataHex;
    r.verdict    = "PENDING";

    beginInsertRows(QModelIndex(), results_.size(), results_.size());
    results_.append(r);
    endInsertRows();

    pendingStimuli_.enqueue(PendingStimulus{stimulusId, traceSeq, rowIndex, dataHex});
}

void FuzzResultsModel::addResponse(quint32 traceSeq, const QString &event,
                                    const QString &dataHex, quint32 timestampUs)
{
    if (!pendingStimuli_.isEmpty()) {
        PendingStimulus pendingStimulus = pendingStimuli_.dequeue();

        if (pendingStimulus.rowIndex >= 0 && pendingStimulus.rowIndex < results_.size()) {
            FuzzResult &r = results_[pendingStimulus.rowIndex];
            r.stimulusId = pendingStimulus.stimulusId;
            r.traceSeq   = pendingStimulus.traceSeq;
            r.responseEvent       = event;
            r.responseData        = dataHex;
            r.responseTimestampUs = timestampUs;
            r.verdict             = (event == "NACK") ? "NACK"
                                  : (event == "OVERFLOW") ? "ERROR"
                                  : "OK";

            emit dataChanged(index(pendingStimulus.rowIndex, 0),
                             index(pendingStimulus.rowIndex, ColumnCount - 1));
            return;
        }

        FuzzResult r;
        r.stimulusId = pendingStimulus.stimulusId;
        r.traceSeq   = pendingStimulus.traceSeq;
        r.sentData   = pendingStimulus.dataHex;
        r.responseEvent       = event;
        r.responseData        = dataHex;
        r.responseTimestampUs = timestampUs;
        r.verdict             = (event == "NACK") ? "NACK"
                              : (event == "OVERFLOW") ? "ERROR"
                              : "OK";

        beginInsertRows(QModelIndex(), results_.size(), results_.size());
        results_.append(r);
        endInsertRows();
        return;
    }

    pendingResponses_.enqueue(PendingResponse{event, dataHex, timestampUs});
}

void FuzzResultsModel::finaliseTimeouts()
{
    for (int i = 0; i < results_.size(); ++i) {
        if (results_[i].verdict == "PENDING") {
            results_[i].verdict       = "TIMEOUT";
            results_[i].responseEvent = "—";
            emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
        }
    }
}

void FuzzResultsModel::clear()
{
    beginResetModel();
    results_.clear();
    pendingStimuli_.clear();
    pendingResponses_.clear();
    endResetModel();
}

int FuzzResultsModel::totalCount() const   { return results_.size(); }

int FuzzResultsModel::okCount() const
{
    int n = 0;
    for (const auto &r : results_) if (r.verdict == "OK") ++n;
    return n;
}

int FuzzResultsModel::nackCount() const
{
    int n = 0;
    for (const auto &r : results_) if (r.verdict == "NACK") ++n;
    return n;
}

int FuzzResultsModel::timeoutCount() const
{
    int n = 0;
    for (const auto &r : results_) if (r.verdict == "TIMEOUT") ++n;
    return n;
}

int FuzzResultsModel::errorCount() const
{
    int n = 0;
    for (const auto &r : results_) if (r.verdict == "ERROR") ++n;
    return n;
}
