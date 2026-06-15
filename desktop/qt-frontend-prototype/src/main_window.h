#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QByteArray>
#include <QMainWindow>
#include <QVector>

#include "fuzz_orchestrator.h"

class FuzzOrchestrator;
class FuzzResultsModel;

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QTableView;
class QTableWidget;
class QTimer;
class PicoBackend;

struct TraceRecord {
    int sequence;
    QString timestamp;
    QString bus;
    QString event;
    int dataLength;
    QString data;
    QString decoded;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void handleBackendFrame(int type, quint16 sessionId, quint32 sequence, const QByteArray &payload);
    void connectDevice();
    void readCapabilities();
    void armDevice();
    void startCapture();
    void stopActivity();
    void disarmDevice();
    void queueStimulus();
    void startFuzz();
    void clearTrace();
    void addDemoFrame();
    void updateFrequencyLabel(int value);
    void applyTraceFilter(const QString &text);
    void runFuzzSession();
    void stopFuzzSession();
    void onFuzzFinished(int total, int ok, int nack, int timeout);
    void addCorpusEntry();
    void removeCorpusEntry();
    void clearCorpus();
    void clearFuzzResults();

private:
    enum class DeviceState {
        Detached,
        Connected,
        CapabilitiesRead,
        Armed,
        Capturing,
        Fuzzing,
        Disarmed
    };

    QWidget *buildCentralWidget();
    QGroupBox *buildConnectionPanel();
    QGroupBox *buildTracePanel();
    QGroupBox *buildFuzzerPanel();
    QWidget *formRow(const QString &labelText, QWidget *field);
    QWidget *buttonRow(const QList<QPushButton *> &buttons);
    void configureComboBox(QComboBox *comboBox);
    void setupTraceTable();
    void addSessionLog(const QString &message);
    void addTraceRecord(const TraceRecord &record);
    void renderTraceTable();
    void setDeviceState(DeviceState nextState);
    void updateStatus();
    void updateControlAvailability();
    void startTimerForMode(const QString &mode);
    void stopTimer();
    void addGeneratedCaptureFrame();
    void addGeneratedFuzzFrame();
    int byteCountForData(const QString &data) const;
    QString currentTimestamp() const;
    QString selectedBus() const;
    QString decodeCaptureEvent(const QString &event, const QString &data) const;

    QLabel *stateLabel_ = nullptr;
    QLabel *sessionLabel_ = nullptr;
    QLabel *frameCountLabel_ = nullptr;
    QLabel *queueLabel_ = nullptr;
    QLabel *rxOverrunsLabel_ = nullptr;
    QLabel *txUnderrunsLabel_ = nullptr;
    QLabel *frequencyValueLabel_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *capsButton_ = nullptr;
    QPushButton *armButton_ = nullptr;
    QPushButton *captureButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QPushButton *disarmButton_ = nullptr;
    QPushButton *queueButton_ = nullptr;
    QPushButton *fuzzButton_ = nullptr;
    QPushButton *addDemoFrameButton_ = nullptr;
    QComboBox *protocolCombo_ = nullptr;
    QComboBox *portCombo_ = nullptr;
    QComboBox *baudrateCombo_ = nullptr;
    QComboBox *parityCombo_ = nullptr;
    QComboBox *i2cSpeedCombo_ = nullptr;
    QComboBox *pullupCombo_ = nullptr;
    QComboBox *vtargetCombo_ = nullptr;
    QComboBox *attackCombo_ = nullptr;
    QComboBox *selectionCombo_ = nullptr;
    QLineEdit *i2cAddressEdit_ = nullptr;
    QLineEdit *pinAEdit_ = nullptr;
    QLineEdit *pinBEdit_ = nullptr;
    QLineEdit *stimulusEdit_ = nullptr;
    QLineEdit *repeatEdit_ = nullptr;
    QLineEdit *budgetEdit_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QListWidget *sessionLog_ = nullptr;
    QSlider *frequencySlider_ = nullptr;
    QTableWidget *traceTable_ = nullptr;
    QTimer *timer_ = nullptr;
    PicoBackend *backend_ = nullptr;
    FuzzOrchestrator *fuzzOrchestrator_ = nullptr;
    FuzzResultsModel *fuzzResultsModel_ = nullptr;

    QVector<TraceRecord> traceRecords_;
    DeviceState deviceState_ = DeviceState::Detached;
    QString timerMode_ = "idle";
    int sequence_ = 0;
    int queuedStimuli_ = 0;
    int rxOverruns_ = 0;
    int txUnderruns_ = 0;
    int sessionId_ = 0;
    uint8_t supportedModes_ = 0;
    bool fuzzSupported_ = false;

    /* Fuzzer panel widgets */
    QListWidget *corpusList_ = nullptr;
    QTableView *fuzzResultsView_ = nullptr;
    QPushButton *runFuzzButton_ = nullptr;
    QPushButton *stopFuzzButton_ = nullptr;
    QPushButton *addCorpusButton_ = nullptr;
    QPushButton *removeCorpusButton_ = nullptr;
    QPushButton *clearResultsButton_ = nullptr;
    QLabel *fuzzStatsLabel_ = nullptr;
    QCheckBox *bitFlipCheck_ = nullptr;
    QCheckBox *truncateCheck_ = nullptr;
    QCheckBox *corruptParityCheck_ = nullptr;
    QCheckBox *badStopCheck_ = nullptr;
    QCheckBox *timingDistortCheck_ = nullptr;
    QCheckBox *i2cSkipAckCheck_ = nullptr;
    QCheckBox *i2cRepeatedStartCheck_ = nullptr;
    QCheckBox *i2cClockStretchCheck_ = nullptr;
    QVector<CorpusEntry> corpusEntries_;
};

/* Forward-declare CorpusEntry so main_window.h sees it */
#include "fuzz_orchestrator.h"

#endif
