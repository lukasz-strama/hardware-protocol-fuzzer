#include "main_window.h"
#include "backend_bridge.h"
#include "fuzz_orchestrator.h"
#include "fuzz_results_model.h"

#include <QAbstractItemView>
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
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QTableView>

namespace {

class DropdownComboBox final : public QComboBox
{
public:
    explicit DropdownComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover, true);
    }

    QSize sizeHint() const override
    {
        QSize size = QComboBox::sizeHint();
        size.setHeight(34);
        return size;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor border = hasFocus()
            ? QColor("#2563eb")
            : (underMouse() ? QColor("#93b4e8") : QColor("#c7d0dd"));
        const QColor background = isEnabled() ? QColor("#ffffff") : QColor("#f8fafc");
        const QColor text = isEnabled() ? QColor("#1d2430") : QColor("#98a2b3");

        painter.setPen(QPen(border, 1));
        painter.setBrush(background);
        painter.drawRoundedRect(box, 5, 5);

        const QRectF arrowArea(width() - 34, 0, 34, height());
        painter.setPen(QPen(QColor("#d7dee8"), 1));
        painter.drawLine(QPointF(arrowArea.left(), 7), QPointF(arrowArea.left(), height() - 7));

        const qreal centerX = arrowArea.center().x();
        const qreal centerY = arrowArea.center().y() + 1;
        QPainterPath chevron;
        chevron.moveTo(centerX - 5, centerY - 3);
        chevron.lineTo(centerX, centerY + 2);
        chevron.lineTo(centerX + 5, centerY - 3);
        painter.setPen(QPen(text, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(chevron);

        const QRect textRect = rect().adjusted(10, 0, -40, 0);
        painter.setPen(text);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, currentText());
    }
};

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
      timer_(new QTimer(this)),
      backend_(new PicoBackend(this)),
      fuzzResultsModel_(new FuzzResultsModel(this))
{
    fuzzOrchestrator_ = new FuzzOrchestrator(backend_, fuzzResultsModel_, this);

    setWindowTitle("Hardware Protocol Fuzzer - Qt Desktop Prototype");
    setCentralWidget(buildCentralWidget());
    statusBar()->showMessage("Real backend: USB CDC serial session to Pico");

    connect(backend_, &PicoBackend::frameReceived, this, &MainWindow::handleBackendFrame);
    connect(backend_, &PicoBackend::backendUpdated, this, &MainWindow::updateStatus);
    connect(backend_, &PicoBackend::logMessage, this, [this](const QString &message) {
        addSessionLog(message);
    });

    connect(timer_, &QTimer::timeout, this, [this]() {
        if (backend_) {
            backend_->pump();
        }
    });

    connect(fuzzOrchestrator_, &FuzzOrchestrator::logMessage, this, [this](const QString &msg) {
        addSessionLog(msg);
    });
    connect(fuzzOrchestrator_, &FuzzOrchestrator::sessionStarted, this, [this]() {
        setDeviceState(DeviceState::Fuzzing);
    });
    connect(fuzzOrchestrator_, &FuzzOrchestrator::sessionStopped,
            this, &MainWindow::onFuzzFinished);

    updateStatus();
}

QWidget *MainWindow::buildCentralWidget()
{
    auto *root = new QWidget;
    root->setObjectName("centralRoot");
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

    auto *mockBanner = new QLabel("Live mode - USB CDC backend connected to the Pico firmware");
    mockBanner->setObjectName("mockBanner");
    mockBanner->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(mockBanner);

    rootLayout->addWidget(splitter, 1);

    root->setStyleSheet(R"(
        QWidget {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            font-size: 13px;
            color: #1d2430;
        }
        QWidget#centralRoot {
            background: #eef1f5;
            color: #1d2430;
        }
        QGroupBox QWidget,
        QGroupBox QLabel {
            background: transparent;
        }
        QFrame#header, QGroupBox {
            background: #ffffff;
            border: 1px solid #d7dee8;
            border-radius: 8px;
        }
        QLabel#appTitle {
            font-size: 21px;
            font-weight: 800;
            color: #1d2430;
        }
        QLabel[muted="true"] {
            color: #667085;
        }
        QLabel#stateLabel {
            border-radius: 12px;
            padding: 4px 10px;
            font-weight: 800;
        }
        QLabel#mockBanner {
            min-height: 34px;
            border: 1px solid #f59e0b;
            border-radius: 8px;
            background: #fffbeb;
            color: #92400e;
            font-weight: 800;
        }
        QGroupBox {
            margin-top: 18px;
            padding: 14px;
            color: #667085;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 5px;
            font-weight: 800;
            color: #667085;
        }
        QLineEdit {
            min-height: 32px;
            max-height: 32px;
            border: 1px solid #c7d0dd;
            border-radius: 5px;
            padding: 4px 7px;
            background: #ffffff;
            color: #1d2430;
            selection-background-color: #dbeafe;
            selection-color: #1d2430;
        }
        QComboBox {
            min-height: 34px;
            max-height: 34px;
            border: 0;
            background: transparent;
            padding: 0;
            color: #1d2430;
        }
        QComboBox:disabled,
        QLineEdit:disabled {
            color: #98a2b3;
        }
        QLineEdit:focus {
            border-color: #93b4e8;
        }
        QComboBox QAbstractItemView {
            border: 1px solid #c7d0dd;
            border-radius: 8px;
            background: #ffffff;
            selection-background-color: transparent;
            selection-color: #1d2430;
            padding: 0;
            outline: 0;
        }
        QListView#comboPopup {
            border: 1px solid #c7d0dd;
            border-radius: 8px;
            background: #ffffff;
            color: #1d2430;
            padding: 4px;
            outline: 0;
            selection-background-color: transparent;
            selection-color: #1d2430;
        }
        QListView#comboPopup::item {
            min-height: 28px;
            padding: 6px 10px;
            margin: 0;
            border-radius: 5px;
            color: #1d2430;
        }
        QListView#comboPopup::item:hover {
            background: #f1f5f9;
        }
        QListView#comboPopup::item:selected {
            background: #dbeafe;
            color: #1d2430;
            font-weight: 700;
        }
        QListView#comboPopup::item:selected:hover {
            background: #c7ddff;
        }
        QScrollBar:vertical,
        QScrollBar:horizontal {
            background: #f8fafc;
        }
        QScrollBar:vertical {
            width: 10px;
            margin: 4px 2px 4px 0;
            background: #f8fafc;
        }
        QScrollBar:horizontal {
            height: 10px;
            margin: 0 4px 2px 4px;
            background: #f8fafc;
        }
        QScrollBar::handle:vertical {
            min-height: 28px;
            border-radius: 5px;
            background: #c7d0dd;
            margin: 2px 0;
        }
        QScrollBar::handle:vertical:hover {
            background: #98a2b3;
        }
        QScrollBar::handle:horizontal {
            min-width: 28px;
            border-radius: 5px;
            background: #c7d0dd;
            margin: 0 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #98a2b3;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical,
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            height: 0;
            width: 0;
            background: transparent;
        }
        QPushButton {
            min-height: 30px;
            border: 1px solid #b9c4d2;
            border-radius: 6px;
            background: #ffffff;
            color: #1d2430;
            padding: 4px 8px;
        }
        QPushButton:hover {
            border-color: #2563eb;
            color: #2563eb;
        }
        QTableWidget,
        QTableView {
            background: #ffffff;
            color: #1d2430;
            border: 1px solid #d7dee8;
            border-radius: 6px;
            gridline-color: #e3e8ef;
            alternate-background-color: #f8fafc;
            selection-background-color: #dbeafe;
            selection-color: #1d2430;
            show-decoration-selected: 1;
            margin: 0;
            padding: 0;
        }
        QTableWidget::item,
        QTableView::item {
            padding: 2px 4px;
            border: none;
            margin: 0;
        }
        QTableView::indicator {
            width: 12px;
            height: 12px;
        }
        QHeaderView {
            background: #f3f6fa;
        }
        QHeaderView::section {
            background: #f3f6fa;
            color: #475467;
            border: none;
            border-right: 1px solid #d7dee8;
            border-bottom: 1px solid #d7dee8;
            padding: 6px 4px;
            font-weight: 800;
            text-align: left;
        }
        QHeaderView::section:last {
            border-right: none;
        }
        QHeaderView::section:hover {
            background: #eef1f5;
        }
        QListWidget#sessionLog {
            min-height: 92px;
            border: 1px solid #d7dee8;
            border-radius: 6px;
            background: #f8fafc;
            color: #475467;
            padding: 4px;
        }
        QListWidget#sessionLog::item {
            min-height: 22px;
            padding: 2px 4px;
        }
    )");

    return root;
}

QGroupBox *MainWindow::buildConnectionPanel()
{
    auto *panel = new QGroupBox("Connection");
    auto *layout = new QVBoxLayout(panel);
    layout->setSpacing(9);

    protocolCombo_ = new DropdownComboBox;
    protocolCombo_->addItems({"UART"});
    configureComboBox(protocolCombo_);

    portCombo_ = new DropdownComboBox;
    portCombo_->addItems({"/dev/ttyACM0", "/dev/ttyUSB0", "/dev/ttyACM1"});
    configureComboBox(portCombo_);

    baudrateCombo_ = new DropdownComboBox;
    baudrateCombo_->addItems({"9600", "115200", "230400", "460800", "921600", "1000000"});
    configureComboBox(baudrateCombo_);

    parityCombo_ = new DropdownComboBox;
    parityCombo_->addItems({"None", "Even", "Odd"});
    configureComboBox(parityCombo_);

    vtargetCombo_ = new DropdownComboBox;
    vtargetCombo_->addItems({"3300 mV", "1800 mV", "5000 mV"});
    configureComboBox(vtargetCombo_);

    pinAEdit_ = new QLineEdit("4");
    pinBEdit_ = new QLineEdit("5");

    layout->addWidget(formRow("Protocol", protocolCombo_));
    layout->addWidget(formRow("Port", portCombo_));
    layout->addWidget(formRow("UART baud", baudrateCombo_));
    layout->addWidget(formRow("Parity", parityCombo_));
    layout->addWidget(formRow("TX", pinAEdit_));
    layout->addWidget(formRow("RX", pinBEdit_));
    layout->addWidget(formRow("Vtarget", vtargetCombo_));

    connectButton_ = new QPushButton("Connect");
    capsButton_ = new QPushButton("Get Caps");
    armButton_ = new QPushButton("Arm");
    captureButton_ = new QPushButton("Start Capture");
    stopButton_ = new QPushButton("Stop");
    disarmButton_ = new QPushButton("Disarm");
    layout->addWidget(buttonRow({connectButton_, capsButton_, armButton_, captureButton_, stopButton_, disarmButton_}));

    layout->addSpacing(4);
    layout->addWidget(smallLabel("Session log"));
    sessionLog_ = new QListWidget;
    sessionLog_->setObjectName("sessionLog");
    sessionLog_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sessionLog_->addItem("Mock transport idle");
    layout->addWidget(sessionLog_);

    layout->addStretch(1);
    layout->addWidget(smallLabel("Frame: 16 B header + payload, CRC-16/CCITT-FALSE"));
    layout->addWidget(smallLabel("Backend: root serial transport, firmware currently supports UART capture"));

    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::connectDevice);
    connect(capsButton_, &QPushButton::clicked, this, &MainWindow::readCapabilities);
    connect(armButton_, &QPushButton::clicked, this, &MainWindow::armDevice);
    connect(captureButton_, &QPushButton::clicked, this, &MainWindow::startCapture);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopActivity);
    connect(disarmButton_, &QPushButton::clicked, this, &MainWindow::disarmDevice);

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

    addDemoFrameButton_ = new QPushButton("Add Mock Frame");
    auto *clearButton = new QPushButton("Clear");
    toolbar->addWidget(filterEdit_);
    toolbar->addWidget(addDemoFrameButton_);
    toolbar->addWidget(clearButton);

    setupTraceTable();

    layout->addLayout(toolbar);
    layout->addWidget(traceTable_, 1);

    connect(addDemoFrameButton_, &QPushButton::clicked, this, &MainWindow::addDemoFrame);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearTrace);
    connect(filterEdit_, &QLineEdit::textChanged, this, &MainWindow::applyTraceFilter);

    return panel;
}

QGroupBox *MainWindow::buildFuzzerPanel()
{
    auto *panel = new QGroupBox("Fuzzer Control");
    auto *layout = new QVBoxLayout(panel);
    layout->setSpacing(12);
    layout->setContentsMargins(12, 12, 12, 12);

    /* ── Bus & Mode ──────────────────────────────────────────── */
    attackCombo_ = new DropdownComboBox;
    attackCombo_->addItems({"Raw bytes", "Address sweep", "Boundary values", "Random bytes"});
    configureComboBox(attackCombo_);

    selectionCombo_ = new DropdownComboBox;
    selectionCombo_->addItems({"Sequential", "Random", "Corpus-guided"});
    configureComboBox(selectionCombo_);

    budgetEdit_ = new QLineEdit("5000");
    stimulusEdit_ = new QLineEdit("A0 00 FF 13 37");

    layout->addWidget(formRow("Attack", attackCombo_));
    layout->addWidget(formRow("Selection", selectionCombo_));
    layout->addWidget(formRow("Budget ms", budgetEdit_));

    /* ── Separator ────────────────────────────────────────────── */
    auto *sep1 = new QFrame;
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    sep1->setStyleSheet("color: #d7dee8;");
    layout->addWidget(sep1);

    /* ── Policy flags (checkboxes) ────────────────────────────── */
    auto *flagsLabel = smallLabel("Mutation & Error Injection");
    layout->addWidget(flagsLabel);

    bitFlipCheck_          = new QCheckBox("Bit flip");
    truncateCheck_         = new QCheckBox("Truncate");
    corruptParityCheck_    = new QCheckBox("Corrupt parity (UART)");
    badStopCheck_          = new QCheckBox("Bad stop bit (UART)");
    timingDistortCheck_    = new QCheckBox("Timing distort (UART)");
    i2cSkipAckCheck_       = new QCheckBox("Skip ACK (I2C)");
    i2cRepeatedStartCheck_ = new QCheckBox("Repeated START (I2C)");
    i2cClockStretchCheck_  = new QCheckBox("Clock stretch (I2C)");

    auto *flagsGrid = new QGridLayout;
    flagsGrid->setSpacing(6);
    flagsGrid->setContentsMargins(0, 0, 0, 0);
    flagsGrid->addWidget(bitFlipCheck_, 0, 0);
    flagsGrid->addWidget(truncateCheck_, 0, 1);
    flagsGrid->addWidget(corruptParityCheck_, 1, 0);
    flagsGrid->addWidget(badStopCheck_, 1, 1);
    flagsGrid->addWidget(timingDistortCheck_, 2, 0);
    flagsGrid->addWidget(i2cSkipAckCheck_, 2, 1);
    flagsGrid->addWidget(i2cRepeatedStartCheck_, 3, 0);
    flagsGrid->addWidget(i2cClockStretchCheck_, 3, 1);
    layout->addLayout(flagsGrid);

    /* ── Separator ────────────────────────────────────────────── */
    auto *sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    sep2->setStyleSheet("color: #d7dee8;");
    layout->addWidget(sep2);

    /* ── Corpus ───────────────────────────────────────────────── */
    layout->addWidget(smallLabel("Corpus (hex bytes per entry)"));
    layout->addWidget(formRow("Stimulus", stimulusEdit_));

    auto *corpusButtons = new QHBoxLayout;
    addCorpusButton_ = new QPushButton("Add");
    removeCorpusButton_ = new QPushButton("Remove");
    corpusButtons->addWidget(addCorpusButton_);
    corpusButtons->addWidget(removeCorpusButton_);
    layout->addLayout(corpusButtons);

    corpusList_ = new QListWidget;
    corpusList_->setObjectName("sessionLog");
    corpusList_->setMaximumHeight(80);
    layout->addWidget(corpusList_);

    /* ── Separator ────────────────────────────────────────────── */
    auto *sep3 = new QFrame;
    sep3->setFrameShape(QFrame::HLine);
    sep3->setFrameShadow(QFrame::Sunken);
    sep3->setStyleSheet("color: #d7dee8;");
    layout->addWidget(sep3);

    /* ── Run/Stop buttons ────────────────────────────────────── */
    runFuzzButton_ = new QPushButton("▶ Run Fuzz");
    stopFuzzButton_ = new QPushButton("■ Stop Fuzz");
    clearResultsButton_ = new QPushButton("Clear Results");
    auto *runStopLayout = new QHBoxLayout;
    runStopLayout->setSpacing(8);
    runStopLayout->addWidget(runFuzzButton_);
    runStopLayout->addWidget(stopFuzzButton_);
    runStopLayout->addWidget(clearResultsButton_);
    layout->addLayout(runStopLayout);

    /* ── Results table ────────────────────────────────────────── */
    layout->addWidget(smallLabel("Fuzz Results"));
    fuzzResultsView_ = new QTableView;
    fuzzResultsView_->setModel(fuzzResultsModel_);
    fuzzResultsView_->setAlternatingRowColors(true);
    fuzzResultsView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fuzzResultsView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fuzzResultsView_->verticalHeader()->setVisible(false);
    fuzzResultsView_->verticalHeader()->setDefaultSectionSize(24);
    fuzzResultsView_->horizontalHeader()->setStretchLastSection(true);
    fuzzResultsView_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    fuzzResultsView_->setMinimumHeight(100);
    fuzzResultsView_->setShowGrid(true);
    fuzzResultsView_->setGridStyle(Qt::SolidLine);
    layout->addWidget(fuzzResultsView_, 1);

    /* ── Separator ────────────────────────────────────────────── */
    auto *sep4 = new QFrame;
    sep4->setFrameShape(QFrame::HLine);
    sep4->setFrameShadow(QFrame::Sunken);
    sep4->setStyleSheet("color: #d7dee8;");
    layout->addWidget(sep4);

    /* ── Stats ────────────────────────────────────────────────── */
    fuzzStatsLabel_ = smallLabel("Total: 0  |  OK: 0  |  NACK: 0  |  Timeout: 0");
    layout->addWidget(fuzzStatsLabel_);

    queueLabel_ = smallLabel("Queued: 0");
    rxOverrunsLabel_ = smallLabel("RX overruns: 0");
    txUnderrunsLabel_ = smallLabel("TX underruns: 0");
    layout->addWidget(queueLabel_);
    layout->addWidget(rxOverrunsLabel_);
    layout->addWidget(txUnderrunsLabel_);
    layout->addWidget(smallLabel("Limits: max_pending ≤ 32, pending_bytes ≤ 4096"));

    /* ── Connections ──────────────────────────────────────────── */
    connect(addCorpusButton_, &QPushButton::clicked, this, &MainWindow::addCorpusEntry);
    connect(removeCorpusButton_, &QPushButton::clicked, this, &MainWindow::removeCorpusEntry);
    connect(runFuzzButton_, &QPushButton::clicked, this, &MainWindow::runFuzzSession);
    connect(stopFuzzButton_, &QPushButton::clicked, this, &MainWindow::stopFuzzSession);
    connect(clearResultsButton_, &QPushButton::clicked, this, &MainWindow::clearFuzzResults);

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
    label->setFixedHeight(32);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    field->setMinimumWidth(150);
    if (qobject_cast<QComboBox *>(field) || qobject_cast<QLineEdit *>(field)) {
        field->setFixedHeight(32);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

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

void MainWindow::configureComboBox(QComboBox *comboBox)
{
    auto *popup = new QListView(comboBox);
    popup->setObjectName("comboPopup");
    popup->setAutoFillBackground(true);
    popup->setUniformItemSizes(true);
    popup->setMouseTracking(true);
    popup->setFrameShape(QFrame::NoFrame);
    popup->setEditTriggers(QAbstractItemView::NoEditTriggers);
    popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popup->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    popup->setSpacing(0);
    popup->setContentsMargins(0, 0, 0, 0);
    popup->viewport()->setAutoFillBackground(true);
    popup->viewport()->setCursor(Qt::PointingHandCursor);
    popup->viewport()->setStyleSheet("background: #ffffff;");

    QPalette palette = popup->palette();
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::Window, QColor("#ffffff"));
    palette.setColor(QPalette::Text, QColor("#1d2430"));
    palette.setColor(QPalette::HighlightedText, QColor("#1d2430"));
    palette.setColor(QPalette::Highlight, QColor("#dbeafe"));
    popup->setPalette(palette);
    popup->viewport()->setPalette(palette);

    comboBox->setView(popup);
    comboBox->setCursor(Qt::PointingHandCursor);
    comboBox->setMaxVisibleItems(8);
    comboBox->setMinimumContentsLength(12);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
}

void MainWindow::setupTraceTable()
{
    traceTable_ = new QTableWidget(0, 7);
    traceTable_->setHorizontalHeaderLabels({"Seq", "Time", "Bus", "Event", "Len", "Data", "Decoded"});
    traceTable_->setAlternatingRowColors(true);
    traceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    traceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    traceTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    traceTable_->verticalHeader()->setVisible(false);
    traceTable_->horizontalHeader()->setStretchLastSection(false);
    traceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    traceTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
}

void MainWindow::addTraceRecord(const TraceRecord &record)
{
    traceRecords_.append(record);
    renderTraceTable();
}

void MainWindow::addSessionLog(const QString &message)
{
    if (!sessionLog_) {
        return;
    }

    sessionLog_->addItem(QString("%1  %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(message));
    sessionLog_->scrollToBottom();
}

void MainWindow::renderTraceTable()
{
    const QString filter = filterEdit_ ? filterEdit_->text().trimmed().toLower() : QString();
    traceTable_->setRowCount(0);

    for (const auto &record : traceRecords_) {
        const QString haystack = QString("%1 %2 %3 %4 %5 %6 %7")
            .arg(record.sequence)
            .arg(record.timestamp, record.bus, record.event)
            .arg(record.dataLength)
            .arg(record.data, record.decoded)
            .toLower();
        if (!filter.isEmpty() && !haystack.contains(filter)) {
            continue;
        }

        const int row = traceTable_->rowCount();
        traceTable_->insertRow(row);
        const QList<QString> values = {
            QString::number(record.sequence),
            record.timestamp,
            record.bus,
            record.event,
            QString::number(record.dataLength),
            record.data,
            record.decoded
        };

        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 3) {
                item->setData(Qt::ForegroundRole, QColor(eventStyle(record.event).contains("#b42318") ? "#b42318" :
                    eventStyle(record.event).contains("#6d28d9") ? "#6d28d9" :
                    eventStyle(record.event).contains("#12805c") ? "#12805c" : "#1d4ed8"));
                item->setData(Qt::FontRole, QFont(QApplication::font().family(), 13, QFont::Bold));
            }
            if (column == 0 || column == 1 || column == 4 || column == 5) {
                item->setFont(QFont("Menlo", 12));
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
    updateControlAvailability();
}

void MainWindow::updateControlAvailability()
{
    const bool detached = deviceState_ == DeviceState::Detached || deviceState_ == DeviceState::Disarmed;
    const bool connected = deviceState_ == DeviceState::Connected;
    const bool capabilitiesRead = deviceState_ == DeviceState::CapabilitiesRead;
    const bool armed = deviceState_ == DeviceState::Armed;
    const bool running = deviceState_ == DeviceState::Capturing || deviceState_ == DeviceState::Fuzzing;
    const bool canEditConfig = !running;

    if (connectButton_) {
        connectButton_->setEnabled(detached);
    }
    if (capsButton_) {
        capsButton_->setEnabled(connected || capabilitiesRead);
    }
    if (armButton_) {
        armButton_->setEnabled(capabilitiesRead || armed);
    }
    if (captureButton_) {
        captureButton_->setEnabled(armed);
    }
    if (stopButton_) {
        stopButton_->setEnabled(running);
    }
    if (disarmButton_) {
        disarmButton_->setEnabled(connected || capabilitiesRead || armed);
    }
    if (runFuzzButton_) {
        runFuzzButton_->setEnabled(armed && fuzzSupported_);
    }
    if (stopFuzzButton_) {
        stopFuzzButton_->setEnabled(deviceState_ == DeviceState::Fuzzing);
    }
    if (addCorpusButton_) {
        addCorpusButton_->setEnabled(!running);
    }
    if (removeCorpusButton_) {
        removeCorpusButton_->setEnabled(!running);
    }
    if (clearResultsButton_) {
        clearResultsButton_->setEnabled(!running);
    }
    if (addDemoFrameButton_) {
        addDemoFrameButton_->setEnabled(running);
    }

    const QList<QWidget *> configWidgets = {
        protocolCombo_, portCombo_, baudrateCombo_, parityCombo_, pinAEdit_, pinBEdit_, vtargetCombo_,
        attackCombo_, selectionCombo_, stimulusEdit_, budgetEdit_
    };
    for (auto *widget : configWidgets) {
        if (widget) {
            widget->setEnabled(canEditConfig);
        }
    }
}

void MainWindow::startTimerForMode(const QString &mode)
{
    timerMode_ = mode;
    timer_->start(20);
}

void MainWindow::stopTimer()
{
    timer_->stop();
    timerMode_ = "idle";
}

void MainWindow::handleBackendFrame(int type, quint16 sessionId, quint32 sequence, const QByteArray &payload)
{
    const msg_type_t msgType = static_cast<msg_type_t>(type);

    sessionId_ = sessionId;

    if (msgType == MSG_HELLO_ACK && payload.size() >= 8) {
        addSessionLog(QString("HELLO_ACK session=0x%1")
            .arg(sessionId_, 4, 16, QLatin1Char('0')).toUpper());
    } else if (msgType == MSG_CAPS_RESPONSE && payload.size() >= 16) {
        supportedModes_ = static_cast<uint8_t>(payload.at(9));
        fuzzSupported_ = (supportedModes_ & HW_PROTOCOL_MODE_FUZZ) != 0;
        addSessionLog(QString("CAPS_RESPONSE modes=0x%1 bus_mask=0x%2")
            .arg(supportedModes_, 2, 16, QLatin1Char('0'))
            .arg(static_cast<unsigned char>(payload.at(8)), 2, 16, QLatin1Char('0')).toUpper());
    } else if (msgType == MSG_ARM_OK && payload.size() >= 4) {
        addSessionLog(QString("ARM_OK session=0x%1 state=%2")
            .arg(sessionId_, 4, 16, QLatin1Char('0'))
            .arg(static_cast<unsigned char>(payload.at(2))));
    } else if (msgType == MSG_STOP_OK && payload.size() >= 8) {
        const quint32 drained = static_cast<quint32>(static_cast<unsigned char>(payload.at(0)))
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(1))) << 8)
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(2))) << 16)
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(3))) << 24);
        addSessionLog(QString("STOP_OK session=0x%1 drained=%2")
            .arg(sessionId_, 4, 16, QLatin1Char('0'))
            .arg(drained));
    } else if (msgType == MSG_STATUS && payload.size() >= 20) {
        rxOverruns_ = static_cast<uint32_t>(static_cast<unsigned char>(payload.at(0)))
            | (static_cast<uint32_t>(static_cast<unsigned char>(payload.at(1))) << 8)
            | (static_cast<uint32_t>(static_cast<unsigned char>(payload.at(2))) << 16)
            | (static_cast<uint32_t>(static_cast<unsigned char>(payload.at(3))) << 24);
        txUnderruns_ = static_cast<uint32_t>(static_cast<unsigned char>(payload.at(4)))
            | (static_cast<uint32_t>(static_cast<unsigned char>(payload.at(5))) << 8)
            | (static_cast<uint32_t>(static_cast<unsigned char>(payload.at(6))) << 16)
            | (static_cast<uint32_t>(static_cast<unsigned char>(payload.at(7))) << 24);
        queuedStimuli_ = static_cast<int>(static_cast<unsigned char>(payload.at(18)));
    } else if (msgType == MSG_TRACE_DECODED && payload.size() >= 12) {
        const quint32 traceSeq = static_cast<quint32>(static_cast<unsigned char>(payload.at(0)))
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(1))) << 8)
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(2))) << 16)
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(3))) << 24);
        const quint32 timestampUs = static_cast<quint32>(static_cast<unsigned char>(payload.at(4)))
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(5))) << 8)
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(6))) << 16)
            | (static_cast<quint32>(static_cast<unsigned char>(payload.at(7))) << 24);
        const quint16 dataLen = static_cast<quint16>(static_cast<unsigned char>(payload.at(8)))
            | (static_cast<quint16>(static_cast<unsigned char>(payload.at(9))) << 8);
        const uint8_t sourceBus = static_cast<uint8_t>(payload.at(10));
        const uint8_t eventType = static_cast<uint8_t>(payload.at(11));

        QString dataHex;
        const int available = qMin<int>(dataLen, payload.size() - 12);
        for (int i = 0; i < available; ++i) {
            dataHex += QString("%1 ")
                .arg(static_cast<unsigned char>(payload.at(12 + i)), 2, 16, QLatin1Char('0'));
        }

        QString eventName = "BYTE";
        switch (eventType) {
            case HW_PROTOCOL_EVENT_START:   eventName = "START"; break;
            case HW_PROTOCOL_EVENT_STOP:    eventName = "STOP"; break;
            case HW_PROTOCOL_EVENT_ACK:     eventName = "ACK"; break;
            case HW_PROTOCOL_EVENT_NACK:    eventName = "NACK"; break;
            case HW_PROTOCOL_EVENT_BREAK:   eventName = "BREAK"; break;
            case HW_PROTOCOL_EVENT_OVERFLOW:eventName = "OVERFLOW"; break;
            default: break;
        }

        QString decoded;
        if (eventType == HW_PROTOCOL_EVENT_BYTE && available > 0) {
            QStringList parts;
            for (int i = 0; i < available; ++i) {
                const unsigned char ch = static_cast<unsigned char>(payload.at(12 + i));
                if (ch >= 0x20 && ch < 0x7F) {
                    parts << QString("'%1'").arg(QChar(ch));
                } else if (ch == 0x0A) {
                    parts << "\\n";
                } else if (ch == 0x0D) {
                    parts << "\\r";
                } else if (ch == 0x09) {
                    parts << "\\t";
                } else if (ch == 0x00) {
                    parts << "NUL";
                } else {
                    parts << QString("\\x%1").arg(ch, 2, 16, QLatin1Char('0'));
                }
            }
            decoded = parts.join(" ");
        } else if (eventType == HW_PROTOCOL_EVENT_OVERFLOW) {
            decoded = "RX FIFO overflow";
        } else if (eventType == HW_PROTOCOL_EVENT_BREAK) {
            decoded = "UART line break";
        } else {
            decoded = eventName;
        }

        addTraceRecord({
            static_cast<int>(traceSeq),
            QString("%1 us").arg(timestampUs),
            sourceBus ? "UART" : "I2C",
            eventName,
            static_cast<int>(dataLen),
            dataHex.trimmed().toUpper(),
            decoded
        });

        if (eventType == HW_PROTOCOL_EVENT_OVERFLOW) {
            ++rxOverruns_;
        }
    } else if (msgType == MSG_ERROR && payload.size() >= 8) {
        const quint16 errorCode = static_cast<quint16>(static_cast<unsigned char>(payload.at(2)))
            | (static_cast<quint16>(static_cast<unsigned char>(payload.at(3))) << 8);
        const quint8 severity = static_cast<quint8>(payload.at(6));
        addSessionLog(QString("ERROR code=0x%1 severity=%2")
            .arg(errorCode, 4, 16, QLatin1Char('0'))
            .arg(static_cast<unsigned>(severity)));
    }

    updateStatus();
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

    addTraceRecord({
        sequence_,
        currentTimestamp(),
        bus,
        event,
        byteCountForData(data),
        data,
        decodeCaptureEvent(event, data)
    });
}

void MainWindow::addGeneratedFuzzFrame()
{
    ++sequence_;
    const int stimulusId = sequence_;
    addTraceRecord({
        sequence_,
        currentTimestamp(),
        selectedBus(),
        "FUZZ_TX",
        byteCountForData(stimulusEdit_->text()),
        QString("id=%1 %2").arg(stimulusId).arg(stimulusEdit_->text()),
        QString("%1, %2").arg(attackCombo_->currentText(), selectionCombo_->currentText())
    });

    if (stimulusId % 4 == 0) {
        ++sequence_;
        addTraceRecord({sequence_, currentTimestamp(), selectedBus(), "NACK", 0, "", "Target rejected mutated frame"});
    }

    if (stimulusId % 13 == 0) {
        ++sequence_;
        ++rxOverruns_;
        addTraceRecord({sequence_, currentTimestamp(), selectedBus(), "OVERFLOW", 0, "", "Backpressure marker"});
    }
}

int MainWindow::byteCountForData(const QString &data) const
{
    int count = 0;
    QRegularExpression bytePattern(R"(\b(?:0x)?[0-9A-Fa-f]{2}\b)");
    auto matches = bytePattern.globalMatch(data);
    while (matches.hasNext()) {
        matches.next();
        ++count;
    }
    return count;
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

    /* Reset fuzz state from any prior session */
    fuzzSupported_ = false;
    supportedModes_ = 0;

    QString errorMessage;
    if (!backend_->openPort(portCombo_->currentText(), baudrateCombo_->currentText().toInt(), &errorMessage)) {
        addSessionLog(QString("Open failed: %1").arg(errorMessage));
        setDeviceState(DeviceState::Detached);
        return;
    }

    if (backend_->sendHello() != PICO_OK || !backend_->waitForState(HW_PROTOCOL_STATE_CONNECTED, HW_PROTOCOL_ARM_TIMEOUT_MS)) {
        addSessionLog("HELLO failed or timed out");
        return;
    }

    sessionId_ = backend_->sessionId();
    setDeviceState(DeviceState::Connected);
    addSessionLog(QString("HELLO -> HELLO_ACK, session=0x%1")
        .arg(sessionId_, 4, 16, QLatin1Char('0')).toUpper());
}

void MainWindow::readCapabilities()
{
    if (backend_->sendGetCaps() != PICO_OK || !backend_->waitForState(HW_PROTOCOL_STATE_CAPABILITIES_READ, HW_PROTOCOL_ARM_TIMEOUT_MS)) {
        addSessionLog("GET_CAPS failed or timed out");
        return;
    }

    supportedModes_ = backend_->supportedModes();
    fuzzSupported_ = (supportedModes_ & HW_PROTOCOL_MODE_FUZZ) != 0;
    setDeviceState(DeviceState::CapabilitiesRead);
    addSessionLog(QString("GET_CAPS -> CAPS_RESPONSE, modes=0x%1")
        .arg(supportedModes_, 2, 16, QLatin1Char('0')).toUpper());
}

void MainWindow::armDevice()
{
    stopTimer();

    hw_protocol_set_bus_t bus = {};
    bus.speed_hz = baudrateCombo_->currentText().toUInt();
    bus.bus_type = HW_PROTOCOL_BUS_UART;
    bus.bus_flags = 0;
    bus.pin_a = static_cast<uint8_t>(pinAEdit_->text().toUInt());
    bus.pin_b = static_cast<uint8_t>(pinBEdit_->text().toUInt());
    bus.uart_parity = static_cast<uint8_t>(parityCombo_->currentIndex());
    bus.uart_stop_bits = 0;

    hw_protocol_set_target_t target = {};
    target.vtarget_mv = static_cast<uint16_t>(vtargetCombo_->currentText().section(' ', 0, 0).toUInt());
    target.pin_dir_mask = 0x00;
    target.pullup_mode = HW_PROTOCOL_PULLUP_NONE;
    target.pullup_mask = 0x00;

    if (backend_->sendSetBus(bus) != PICO_OK ||
        backend_->sendSetTarget(target) != PICO_OK ||
        backend_->sendArm() != PICO_OK ||
        !backend_->waitForState(HW_PROTOCOL_STATE_ARMED, HW_PROTOCOL_ARM_TIMEOUT_MS)) {
        addSessionLog("ARM sequence failed or timed out");
        return;
    }

    setDeviceState(DeviceState::Armed);
    addSessionLog(QString("SET_BUS + SET_TARGET -> ARM_OK, tx=%1 rx=%2, vtarget=%3")
        .arg(pinAEdit_->text(), pinBEdit_->text(), vtargetCombo_->currentText()));
}

void MainWindow::startCapture()
{
    if (backend_->sendStartCapture() != PICO_OK || !backend_->waitForState(HW_PROTOCOL_STATE_RUNNING, HW_PROTOCOL_ARM_TIMEOUT_MS)) {
        addSessionLog("START_CAPTURE failed or timed out");
        return;
    }

    setDeviceState(DeviceState::Capturing);
    addSessionLog("START_CAPTURE, live TRACE_DECODED stream active");
    startTimerForMode("capture");
}

void MainWindow::stopActivity()
{
    if (backend_->sendStop() != PICO_OK || !backend_->waitForState(HW_PROTOCOL_STATE_ARMED, HW_PROTOCOL_STOP_TIMEOUT_MS)) {
        addSessionLog("STOP failed or timed out");
        return;
    }

    stopTimer();
    setDeviceState(DeviceState::Armed);
    addSessionLog("STOP -> STOP_OK, drained from firmware");
}

void MainWindow::disarmDevice()
{
    if (backend_->sendDisarm() != PICO_OK) {
        addSessionLog("DISARM failed");
        return;
    }

    stopTimer();
    queuedStimuli_ = 0;
    fuzzSupported_ = false;
    supportedModes_ = 0;
    setDeviceState(DeviceState::Disarmed);
    addSessionLog("DISARM, pins=HIGH-Z");
    backend_->closePort();
}

void MainWindow::queueStimulus()
{
    /* Legacy — redirects to addCorpusEntry */
    addCorpusEntry();
}

void MainWindow::startFuzz()
{
    /* Legacy — redirects to runFuzzSession */
    runFuzzSession();
}

void MainWindow::addCorpusEntry()
{
    const QString hex = stimulusEdit_->text().trimmed();
    if (hex.isEmpty()) return;

    /* Parse hex string into bytes */
    QByteArray bytes;
    const QStringList tokens = hex.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const auto &tok : tokens) {
        bool ok;
        uint val = tok.toUInt(&ok, 16);
        if (!ok) continue;
        bytes.append(static_cast<char>(val & 0xFF));
    }
    if (bytes.isEmpty()) return;

    static int nextId = 1;
    CorpusEntry entry;
    entry.id = nextId++;
    entry.data = bytes;
    entry.label = hex.toUpper();
    corpusEntries_.append(entry);

    corpusList_->addItem(QString("#%1: %2 (%3 B)")
        .arg(entry.id).arg(entry.label).arg(bytes.size()));
    addSessionLog(QString("Corpus: added #%1, %2 bytes").arg(entry.id).arg(bytes.size()));
}

void MainWindow::removeCorpusEntry()
{
    int row = corpusList_->currentRow();
    if (row < 0 || row >= corpusEntries_.size()) return;

    corpusEntries_.removeAt(row);
    delete corpusList_->takeItem(row);
    addSessionLog(QString("Corpus: removed entry at row %1").arg(row));
}

void MainWindow::clearCorpus()
{
    corpusEntries_.clear();
    corpusList_->clear();
    addSessionLog("Corpus: cleared");
}

void MainWindow::runFuzzSession()
{
    if (corpusEntries_.isEmpty()) {
        addCorpusEntry();  /* auto-add from stimulus field */
    }
    if (corpusEntries_.isEmpty()) {
        addSessionLog("[fuzz] No corpus entries to fuzz");
        return;
    }

    /* Build policy flags from checkboxes */
    uint8_t policyFlags = 0;
    if (bitFlipCheck_ && bitFlipCheck_->isChecked())           policyFlags |= (1u << 0);
    if (truncateCheck_ && truncateCheck_->isChecked())         policyFlags |= (1u << 1);
    if (corruptParityCheck_ && corruptParityCheck_->isChecked()) policyFlags |= (1u << 2);
    if (badStopCheck_ && badStopCheck_->isChecked())           policyFlags |= (1u << 3);
    if (timingDistortCheck_ && timingDistortCheck_->isChecked()) policyFlags |= (1u << 4);
    if (i2cSkipAckCheck_ && i2cSkipAckCheck_->isChecked())     policyFlags |= (1u << 5);
    if (i2cRepeatedStartCheck_ && i2cRepeatedStartCheck_->isChecked()) policyFlags |= (1u << 6);
    if (i2cClockStretchCheck_ && i2cClockStretchCheck_->isChecked())   policyFlags |= (1u << 7);

    uint8_t busType = (protocolCombo_->currentText() == "UART") ? 1 : 0;
    uint8_t selMode = static_cast<uint8_t>(selectionCombo_->currentIndex());
    uint32_t budgetMs = budgetEdit_->text().toUInt();

    fuzzOrchestrator_->startSession(
        corpusEntries_, busType, selMode, 0 /* once */, policyFlags, budgetMs);
}

void MainWindow::stopFuzzSession()
{
    fuzzOrchestrator_->stopSession();
    setDeviceState(DeviceState::Armed);
}

void MainWindow::onFuzzFinished(int total, int ok, int nack, int timeout)
{
    setDeviceState(DeviceState::Armed);
    if (fuzzStatsLabel_) {
        fuzzStatsLabel_->setText(QString("Total: %1  |  OK: %2  |  NACK: %3  |  Timeout: %4")
            .arg(total).arg(ok).arg(nack).arg(timeout));
    }
    addSessionLog(QString("Fuzz session complete: %1 total, %2 OK, %3 NACK, %4 timeout")
        .arg(total).arg(ok).arg(nack).arg(timeout));
}

void MainWindow::clearFuzzResults()
{
    if (fuzzResultsModel_) fuzzResultsModel_->clear();
    if (fuzzStatsLabel_) fuzzStatsLabel_->setText("Total: 0  |  OK: 0  |  NACK: 0  |  Timeout: 0");
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
    if (deviceState_ == DeviceState::Fuzzing) {
        addGeneratedFuzzFrame();
    } else {
        addGeneratedCaptureFrame();
    }
}

void MainWindow::updateFrequencyLabel(int value)
{
    frequencyValueLabel_->setText(QString("%1 Hz").arg(value));
}

void MainWindow::applyTraceFilter(const QString &)
{
    renderTraceTable();
}
