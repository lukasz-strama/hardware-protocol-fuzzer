#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QVector>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;

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

    QVector<TraceRecord> traceRecords_;
    DeviceState deviceState_ = DeviceState::Detached;
    QString timerMode_ = "idle";
    int sequence_ = 0;
    int queuedStimuli_ = 0;
    int rxOverruns_ = 0;
    int txUnderruns_ = 0;
    int sessionId_ = 0;
};

#endif
