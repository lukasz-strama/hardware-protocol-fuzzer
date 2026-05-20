#include "main_window.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString eventStyle(const QString &event)
{
    if (event == "NACK" || event == "OVERFLOW") {
        return "color: #b42318; font-weight: 700;";
    }
    if (event == "FUZZ_TX" || event == "QUEUE") {
        return "color: #6d28d9; font-weight: 700;";
    }
    if (event == "ACK") {
        return "color: #12805c; font-weight: 700;";
    }
    return "color: #1d4ed8; font-weight: 700;";
}

QString byteString(int first, int second)
{
    return QString("0x%1 0x%2")
        .arg(first & 0xff, 2, 16, QLatin1Char('0'))
        .arg(second & 0xff, 2, 16, QLatin1Char('0'))
        .toUpper();
}

QLabel *smallLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("muted", true);
    return label;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      timer_(new QTimer(this))
{
    setWindowTitle("Hardware Protocol Fuzzer - Qt Desktop Prototype");
    setCentralWidget(buildCentralWidget());
    statusBar()->showMessage("Mock transport: protocol frames are generated locally");

    connect(timer_, &QTimer::timeout, this, [this]() {
        if (timerMode_ == "fuzz") {
            addGeneratedFuzzFrame();
        } else {
            addGeneratedCaptureFrame();
        }
    });

    seedTrace();
    updateStatus();
}

QWidget *MainWindow::buildCentralWidget()
{
    auto *root = new QWidget;
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(12);

    auto *header = new QFrame;
    header->setObjectName("header");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);

    auto *titleBox = new QVBoxLayout;
    auto *title = new QLabel("Hardware Protocol Fuzzer");
    title->setObjectName("appTitle");
    auto *subtitle = smallLabel("Qt Widgets frontend prototype: connection config, trace view and fuzzer control");
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);

    auto *statusBox = new QHBoxLayout;
    stateLabel_ = new QLabel("Detached");
    stateLabel_->setObjectName("stateLabel");
    sessionLabel_ = new QLabel("Session: -");
    frameCountLabel_ = new QLabel("Frames: 0");
    statusBox->addWidget(stateLabel_);
    statusBox->addWidget(sessionLabel_);
    statusBox->addWidget(frameCountLabel_);

    headerLayout->addLayout(titleBox, 1);
    headerLayout->addLayout(statusBox);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(buildConnectionPanel());
    splitter->addWidget(buildTracePanel());
    splitter->addWidget(buildFuzzerPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({310, 620, 310});

    rootLayout->addWidget(header);
    rootLayout->addWidget(splitter, 1);

    root->setStyleSheet(R"(
        QWidget {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            font-size: 13px;
        }
        QMainWindow, QWidget {
            background: #eef1f5;
            color: #1d2430;
        }
        QFrame#header, QGroupBox {
            background: #ffffff;
            border: 1px solid #d7dee8;
            border-radius: 8px;
        }
        QLabel#appTitle {
            font-size: 21px;
            font-weight: 800;
        }
        QLabel[muted="true"] {
            color: #667085;
        }
        QLabel#stateLabel {
            border-radius: 12px;
            padding: 4px 10px;
            font-weight: 800;
        }
        QGroupBox {
            margin-top: 18px;
            padding: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 5px;
            font-weight: 800;
        }
        QLineEdit, QComboBox {
            min-height: 28px;
            border: 1px solid #c7d0dd;
            border-radius: 5px;
            padding: 4px 7px;
            background: #ffffff;
        }
        QPushButton {
            min-height: 30px;
            border: 1px solid #b9c4d2;
            border-radius: 6px;
            background: #ffffff;
            padding: 4px 8px;
        }
        QPushButton:hover {
            border-color: #2563eb;
            color: #2563eb;
        }
        QTableWidget {
            background: #ffffff;
            border: 1px solid #d7dee8;
            border-radius: 6px;
            gridline-color: #e3e8ef;
            alternate-background-color: #f8fafc;
        }
        QHeaderView::section {
            background: #f3f6fa;
            border: 0;
            border-bottom: 1px solid #d7dee8;
            padding: 6px;
            font-weight: 800;
        }
    )");

    return root;
}

QGroupBox *MainWindow::buildConnectionPanel()
{
    auto *panel = new QGroupBox("Connection");
    auto *layout = new QVBoxLayout(panel);
    layout->setSpacing(9);

    protocolCombo_ = new QComboBox;
    protocolCombo_->addItems({"I2C", "UART"});

    portCombo_ = new QComboBox;
    portCombo_->addItems({"Mock Pico / USB CDC", "COM3", "/dev/ttyACM0", "/dev/cu.usbmodem1101"});

    baudrateCombo_ = new QComboBox;
    baudrateCombo_->addItems({"115200", "230400", "460800", "921600", "1000000"});

    parityCombo_ = new QComboBox;
    parityCombo_->addItems({"None", "Even", "Odd"});

    i2cSpeedCombo_ = new QComboBox;
    i2cSpeedCombo_->addItems({"100 kHz", "400 kHz", "1 MHz"});
    i2cSpeedCombo_->setCurrentText("400 kHz");

    pullupCombo_ = new QComboBox;
    pullupCombo_->addItems({"External", "None", "Internal test only"});

    vtargetCombo_ = new QComboBox;
    vtargetCombo_->addItems({"3300 mV", "1800 mV", "5000 mV"});

    i2cAddressEdit_ = new QLineEdit("0x48");
    pinAEdit_ = new QLineEdit("4");
    pinBEdit_ = new QLineEdit("5");

    layout->addWidget(formRow("Protocol", protocolCombo_));
    layout->addWidget(formRow("Port", portCombo_));
    layout->addWidget(formRow("UART baud", baudrateCombo_));
    layout->addWidget(formRow("Parity", parityCombo_));
    layout->addWidget(formRow("I2C speed", i2cSpeedCombo_));
    layout->addWidget(formRow("I2C addr", i2cAddressEdit_));
    layout->addWidget(formRow("SDA / TX", pinAEdit_));
    layout->addWidget(formRow("SCL / RX", pinBEdit_));
    layout->addWidget(formRow("Pull-up", pullupCombo_));
    layout->addWidget(formRow("Vtarget", vtargetCombo_));

    auto *connectButton = new QPushButton("Connect");
    auto *capsButton = new QPushButton("Get Caps");
    auto *armButton = new QPushButton("Arm");
    auto *captureButton = new QPushButton("Start Capture");
    auto *stopButton = new QPushButton("Stop");
    auto *disarmButton = new QPushButton("Disarm");
    layout->addWidget(buttonRow({connectButton, capsButton, armButton, captureButton, stopButton, disarmButton}));

    layout->addStretch(1);
    layout->addWidget(smallLabel("Frame: 16 B header + payload, CRC-16/CCITT-FALSE"));
    layout->addWidget(smallLabel("Safety: active pins are disabled before ARM"));

    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectDevice);
    connect(capsButton, &QPushButton::clicked, this, &MainWindow::readCapabilities);
    connect(armButton, &QPushButton::clicked, this, &MainWindow::armDevice);
    connect(captureButton, &QPushButton::clicked, this, &MainWindow::startCapture);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopActivity);
    connect(disarmButton, &QPushButton::clicked, this, &MainWindow::disarmDevice);

    return panel;
}

QGroupBox *MainWindow::buildTracePanel()
{
    auto *panel = new QGroupBox("Captured Frames");
    auto *layout = new QVBoxLayout(panel);
    layout->setSpacing(10);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(smallLabel("TRACE_DECODED stream"));
    toolbar->addStretch(1);

    filterEdit_ = new QLineEdit;
    filterEdit_->setPlaceholderText("Filter event/data");
    filterEdit_->setClearButtonEnabled(true);

    auto *addButton = new QPushButton("Add Frame");
    auto *clearButton = new QPushButton("Clear");
    toolbar->addWidget(filterEdit_);
    toolbar->addWidget(addButton);
    toolbar->addWidget(clearButton);

    setupTraceTable();

    layout->addLayout(toolbar);
    layout->addWidget(traceTable_, 1);

    connect(addButton, &QPushButton::clicked, this, &MainWindow::addDemoFrame);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearTrace);
    connect(filterEdit_, &QLineEdit::textChanged, this, &MainWindow::applyTraceFilter);

    return panel;
}

QGroupBox *MainWindow::buildFuzzerPanel()
{
    auto *panel = new QGroupBox("Fuzzer Control");
    auto *layout = new QVBoxLayout(panel);
    layout->setSpacing(9);

    attackCombo_ = new QComboBox;
    attackCombo_->addItems({"Address sweep", "Malformed length", "Repeated START", "Random bytes"});

    selectionCombo_ = new QComboBox;
    selectionCombo_->addItems({"Sequential", "Random", "Corpus-guided"});

    stimulusEdit_ = new QLineEdit("A0 00 FF 13 37");
    repeatEdit_ = new QLineEdit("25");
    budgetEdit_ = new QLineEdit("5000 ms");

    frequencySlider_ = new QSlider(Qt::Horizontal);
    frequencySlider_->setRange(1, 100);
    frequencySlider_->setValue(10);
    frequencyValueLabel_ = new QLabel("10 Hz");

    queueLabel_ = smallLabel("Queued: 0");
    rxOverrunsLabel_ = smallLabel("RX overruns: 0");
    txUnderrunsLabel_ = smallLabel("TX underruns: 0");

    auto *queueButton = new QPushButton("Queue Stimulus");
    auto *fuzzButton = new QPushButton("Start Fuzz");

    layout->addWidget(formRow("Attack", attackCombo_));
    layout->addWidget(formRow("Selection", selectionCombo_));
    layout->addWidget(formRow("Stimulus", stimulusEdit_));
    layout->addWidget(formRow("Repeats", repeatEdit_));
    layout->addWidget(formRow("Budget", budgetEdit_));
    layout->addWidget(formRow("Frequency", frequencySlider_));
    layout->addWidget(formRow("Rate", frequencyValueLabel_));
    layout->addWidget(buttonRow({queueButton, fuzzButton}));
    layout->addStretch(1);
    layout->addWidget(queueLabel_);
    layout->addWidget(rxOverrunsLabel_);
    layout->addWidget(txUnderrunsLabel_);
    layout->addWidget(smallLabel("Limits: max_pending <= 32, pending_bytes <= 4096"));

    connect(queueButton, &QPushButton::clicked, this, &MainWindow::queueStimulus);
    connect(fuzzButton, &QPushButton::clicked, this, &MainWindow::startFuzz);
    connect(frequencySlider_, &QSlider::valueChanged, this, &MainWindow::updateFrequencyLabel);

    return panel;
}

QWidget *MainWindow::formRow(const QString &labelText, QWidget *field)
{
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *label = smallLabel(labelText);
    label->setMinimumWidth(82);
    field->setMinimumWidth(150);

    layout->addWidget(label);
    layout->addWidget(field, 1);
    return container;
}

QWidget *MainWindow::buttonRow(const QList<QPushButton *> &buttons)
{
    auto *container = new QWidget;
    auto *grid = new QGridLayout(container);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    for (int i = 0; i < buttons.size(); ++i) {
        grid->addWidget(buttons.at(i), i / 2, i % 2);
    }

    return container;
}

void MainWindow::setupTraceTable()
{
    traceTable_ = new QTableWidget(0, 5);
    traceTable_->setHorizontalHeaderLabels({"Time", "Bus", "Event", "Data", "Decoded"});
    traceTable_->setAlternatingRowColors(true);
    traceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    traceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    traceTable_->verticalHeader()->setVisible(false);
    traceTable_->horizontalHeader()->setStretchLastSection(true);
    traceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    traceTable_->setColumnWidth(3, 220);
}

void MainWindow::seedTrace()
{
    addTraceRecord({"00:00.000", "I2C", "START", "", "Bus idle -> transaction"});
    addTraceRecord({"00:00.014", "I2C", "BYTE", "0x90", "Address 0x48 write"});
    addTraceRecord({"00:00.021", "I2C", "ACK", "", "Target acknowledged"});
    addTraceRecord({"00:00.036", "I2C", "BYTE", "0x00 0x7F", "Register payload"});
    addTraceRecord({"00:00.051", "I2C", "STOP", "", "Transaction complete"});
}

void MainWindow::addTraceRecord(const TraceRecord &record)
{
    traceRecords_.append(record);
    renderTraceTable();
}

void MainWindow::renderTraceTable()
{
    const QString filter = filterEdit_ ? filterEdit_->text().trimmed().toLower() : QString();
    traceTable_->setRowCount(0);

    for (const auto &record : traceRecords_) {
        const QString haystack = QString("%1 %2 %3 %4 %5")
            .arg(record.timestamp, record.bus, record.event, record.data, record.decoded)
            .toLower();
        if (!filter.isEmpty() && !haystack.contains(filter)) {
            continue;
        }

        const int row = traceTable_->rowCount();
        traceTable_->insertRow(row);
        const QList<QString> values = {
            record.timestamp, record.bus, record.event, record.data, record.decoded
        };

        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 2) {
                item->setData(Qt::ForegroundRole, QColor(eventStyle(record.event).contains("#b42318") ? "#b42318" :
                    eventStyle(record.event).contains("#6d28d9") ? "#6d28d9" :
                    eventStyle(record.event).contains("#12805c") ? "#12805c" : "#1d4ed8"));
                item->setData(Qt::FontRole, QFont(QApplication::font().family(), 13, QFont::Bold));
            }
            traceTable_->setItem(row, column, item);
        }
    }

    if (traceTable_->rowCount() > 0) {
        traceTable_->scrollToBottom();
    }

    updateStatus();
}

void MainWindow::setDeviceState(DeviceState nextState)
{
    deviceState_ = nextState;
    updateStatus();
}

void MainWindow::updateStatus()
{
    QString stateText = "Detached";
    QString stateColor = "#4b5563";
    QString stateBackground = "#f3f4f6";

    switch (deviceState_) {
    case DeviceState::Detached:
        break;
    case DeviceState::Connected:
        stateText = "Connected";
        stateColor = "#1d4ed8";
        stateBackground = "#dbeafe";
        break;
    case DeviceState::CapabilitiesRead:
        stateText = "Capabilities read";
        stateColor = "#1d4ed8";
        stateBackground = "#dbeafe";
        break;
    case DeviceState::Armed:
        stateText = queuedStimuli_ > 0 ? "Armed, stimuli queued" : "Armed";
        stateColor = queuedStimuli_ > 0 ? "#b45309" : "#1d4ed8";
        stateBackground = queuedStimuli_ > 0 ? "#ffedd5" : "#dbeafe";
        break;
    case DeviceState::Capturing:
        stateText = "Capturing";
        stateColor = "#166534";
        stateBackground = "#dcfce7";
        break;
    case DeviceState::Fuzzing:
        stateText = "Running fuzz";
        stateColor = "#6d28d9";
        stateBackground = "#ede9fe";
        break;
    case DeviceState::Disarmed:
        stateText = "Disarmed";
        break;
    }

    stateLabel_->setText(stateText);
    stateLabel_->setStyleSheet(QString("background: %1; color: %2;").arg(stateBackground, stateColor));
    sessionLabel_->setText(sessionId_ > 0
        ? QString("Session: 0x%1").arg(sessionId_, 4, 16, QLatin1Char('0')).toUpper()
        : "Session: -");
    frameCountLabel_->setText(QString("Frames: %1").arg(traceRecords_.size()));
    queueLabel_->setText(QString("Queued: %1").arg(queuedStimuli_));
    rxOverrunsLabel_->setText(QString("RX overruns: %1").arg(rxOverruns_));
    txUnderrunsLabel_->setText(QString("TX underruns: %1").arg(txUnderruns_));
}

void MainWindow::startTimerForMode(const QString &mode)
{
    timerMode_ = mode;
    timer_->start(700);
}

void MainWindow::stopTimer()
{
    timer_->stop();
    timerMode_ = "idle";
}

void MainWindow::addGeneratedCaptureFrame()
{
    ++sequence_;
    const QString bus = selectedBus();
    const QStringList i2cEvents = {"START", "BYTE", "ACK", "BYTE", "STOP"};
    const QStringList uartEvents = {"BYTE", "BYTE", "BYTE", "BREAK", "BYTE"};
    const QString event = bus == "I2C"
        ? i2cEvents.at(sequence_ % i2cEvents.size())
        : uartEvents.at(sequence_ % uartEvents.size());
    const QString data = event == "BYTE"
        ? byteString(0x40 + sequence_, 0xa0 + sequence_ * 3)
        : QString();

    addTraceRecord({currentTimestamp(), bus, event, data, decodeCaptureEvent(event, data)});
}

void MainWindow::addGeneratedFuzzFrame()
{
    ++sequence_;
    const int stimulusId = sequence_;
    addTraceRecord({
        currentTimestamp(),
        selectedBus(),
        "FUZZ_TX",
        QString("id=%1 %2").arg(stimulusId).arg(stimulusEdit_->text()),
        QString("%1, %2").arg(attackCombo_->currentText(), selectionCombo_->currentText())
    });

    if (stimulusId % 4 == 0) {
        addTraceRecord({currentTimestamp(), selectedBus(), "NACK", "", "Target rejected mutated frame"});
    }

    if (stimulusId % 13 == 0) {
        ++rxOverruns_;
        addTraceRecord({currentTimestamp(), selectedBus(), "OVERFLOW", "", "Backpressure marker"});
    }
}

QString MainWindow::currentTimestamp() const
{
    const int totalMs = sequence_ * 137;
    return QString("00:%1.%2")
        .arg((totalMs / 1000) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalMs % 1000, 3, 10, QLatin1Char('0'));
}

QString MainWindow::selectedBus() const
{
    return protocolCombo_->currentText();
}

QString MainWindow::decodeCaptureEvent(const QString &event, const QString &data) const
{
    if (event == "START") {
        return "Bus idle -> transaction";
    }
    if (event == "STOP") {
        return "Transaction complete";
    }
    if (event == "ACK") {
        return "Target acknowledged";
    }
    if (event == "BREAK") {
        return "UART line break detected";
    }
    if (selectedBus() == "I2C") {
        return data.startsWith("0x9") ? QString("Address %1").arg(i2cAddressEdit_->text()) : "Decoded payload chunk";
    }
    return QString("%1 baud, %2 parity").arg(baudrateCombo_->currentText(), parityCombo_->currentText().toLower());
}

void MainWindow::connectDevice()
{
    stopTimer();
    sessionId_ = 0x42;
    setDeviceState(DeviceState::Connected);
    addTraceRecord({"00:00.100", "USB", "HELLO_ACK", "protocol=1 session=0x0042", "Mock Pico connected"});
}

void MainWindow::readCapabilities()
{
    setDeviceState(DeviceState::CapabilitiesRead);
    addTraceRecord({"00:00.140", "USB", "CAPS", "I2C UART PIO=8 BUF=128KiB", "Capabilities received"});
}

void MainWindow::armDevice()
{
    stopTimer();
    setDeviceState(DeviceState::Armed);
    addTraceRecord({
        "00:00.180",
        "USB",
        "ARM_OK",
        QString("pins=%1/%2 vtarget=%3").arg(pinAEdit_->text(), pinBEdit_->text(), vtargetCombo_->currentText()),
        "Configuration validated"
    });
}

void MainWindow::startCapture()
{
    setDeviceState(DeviceState::Capturing);
    addTraceRecord({currentTimestamp(), selectedBus(), "START", "START_CAPTURE", "Live capture mock started"});
    startTimerForMode("capture");
}

void MainWindow::stopActivity()
{
    stopTimer();
    setDeviceState(DeviceState::Armed);
    addTraceRecord({currentTimestamp(), "USB", "STOP_OK", "drained=512", "Buffers drained, back to armed"});
}

void MainWindow::disarmDevice()
{
    stopTimer();
    queuedStimuli_ = 0;
    setDeviceState(DeviceState::Disarmed);
    addTraceRecord({currentTimestamp(), "USB", "DISARM", "pins=HIGH-Z", "Safe state"});
}

void MainWindow::queueStimulus()
{
    queuedStimuli_ = qMin(32, queuedStimuli_ + 1);
    if (deviceState_ != DeviceState::Capturing && deviceState_ != DeviceState::Fuzzing) {
        setDeviceState(DeviceState::Armed);
    }
    addTraceRecord({"00:00.320", "USB", "QUEUE", stimulusEdit_->text(), QString("%1 stimulus queued").arg(attackCombo_->currentText())});
}

void MainWindow::startFuzz()
{
    if (queuedStimuli_ == 0) {
        queueStimulus();
    }
    setDeviceState(DeviceState::Fuzzing);
    addTraceRecord({currentTimestamp(), selectedBus(), "START", "START_FUZZ", "Fuzzer scheduler mock started"});
    startTimerForMode("fuzz");
}

void MainWindow::clearTrace()
{
    traceRecords_.clear();
    sequence_ = 0;
    rxOverruns_ = 0;
    txUnderruns_ = 0;
    renderTraceTable();
}

void MainWindow::addDemoFrame()
{
    addGeneratedCaptureFrame();
}

void MainWindow::updateFrequencyLabel(int value)
{
    frequencyValueLabel_->setText(QString("%1 Hz").arg(value));
}

void MainWindow::applyTraceFilter(const QString &)
{
    renderTraceTable();
}
