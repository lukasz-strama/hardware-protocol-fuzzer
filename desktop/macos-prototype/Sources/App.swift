import Cocoa

struct TraceRecord {
    let timestamp: String
    let bus: String
    let event: String
    let data: String
    let note: String
}

final class TraceTableDataSource: NSObject, NSTableViewDataSource, NSTableViewDelegate {
    var records: [TraceRecord] = []

    func numberOfRows(in tableView: NSTableView) -> Int {
        records.count
    }

    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard let identifier = tableColumn?.identifier.rawValue else { return nil }

        let label = NSTextField(labelWithString: value(for: identifier, row: row))
        label.lineBreakMode = .byTruncatingTail
        label.font = identifier == "data"
            ? NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
            : NSFont.systemFont(ofSize: 12)
        label.textColor = color(for: identifier, row: row)
        label.translatesAutoresizingMaskIntoConstraints = false

        let cell = NSTableCellView()
        cell.addSubview(label)
        NSLayoutConstraint.activate([
            label.leadingAnchor.constraint(equalTo: cell.leadingAnchor, constant: 8),
            label.trailingAnchor.constraint(equalTo: cell.trailingAnchor, constant: -8),
            label.centerYAnchor.constraint(equalTo: cell.centerYAnchor)
        ])
        return cell
    }

    private func value(for column: String, row: Int) -> String {
        let record = records[row]
        switch column {
        case "timestamp":
            return record.timestamp
        case "bus":
            return record.bus
        case "event":
            return record.event
        case "data":
            return record.data
        case "note":
            return record.note
        default:
            return ""
        }
    }

    private func color(for column: String, row: Int) -> NSColor {
        guard column == "event" else { return .labelColor }
        switch records[row].event {
        case "NACK", "OVERFLOW":
            return .systemRed
        case "START", "STOP":
            return .systemBlue
        case "FUZZ_TX":
            return .systemPurple
        default:
            return .labelColor
        }
    }
}

private enum DeviceState {
    case detached
    case connected
    case capabilitiesRead
    case armed
    case capturing
    case fuzzing
    case disarmed
}

final class MainViewController: NSViewController {
    private let dataSource = TraceTableDataSource()
    private let tableView = NSTableView()
    private let statusLabel = NSTextField(labelWithString: "Detached")
    private let sessionLabel = NSTextField(labelWithString: "Session: -")
    private let countersLabel = NSTextField(labelWithString: "RX overruns: 0   TX underruns: 0   queued: 0")
    private let rateLabel = NSTextField(labelWithString: "10 Hz")
    private let timelineLabel = NSTextField(labelWithString: "0 frames")
    private let busControl = NSSegmentedControl(labels: ["I2C", "UART"], trackingMode: .selectOne, target: nil, action: nil)
    private let portPopup = NSPopUpButton()
    private let baudPopup = NSPopUpButton()
    private let i2cSpeedPopup = NSPopUpButton()
    private let addressField = NSTextField(string: "0x48")
    private let sdaField = NSTextField(string: "4")
    private let sclField = NSTextField(string: "5")
    private let attackPopup = NSPopUpButton()
    private let modePopup = NSPopUpButton()
    private let repeatField = NSTextField(string: "25")
    private let stimulusField = NSTextField(string: "A0 00 FF 13 37")
    private let frequencySlider = NSSlider(value: 10, minValue: 1, maxValue: 100, target: nil, action: nil)
    private let connectButton = NSButton(title: "Connect", target: nil, action: nil)
    private let capsButton = NSButton(title: "Get Caps", target: nil, action: nil)
    private let armButton = NSButton(title: "Arm", target: nil, action: nil)
    private let captureButton = NSButton(title: "Start Capture", target: nil, action: nil)
    private let stopButton = NSButton(title: "Stop", target: nil, action: nil)
    private let disarmButton = NSButton(title: "Disarm", target: nil, action: nil)
    private let queueButton = NSButton(title: "Queue Stimulus", target: nil, action: nil)
    private let fuzzButton = NSButton(title: "Start Fuzz", target: nil, action: nil)
    private var timer: Timer?
    private var frameSequence = 0
    private var queuedStimuli = 0
    private var sessionID = 0x42
    private var rxOverruns = 0
    private var txUnderruns = 0
    private var captureRunning = false
    private var fuzzRunning = false
    private var deviceState: DeviceState = .detached

    override func loadView() {
        view = NSView()
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        configureControls()
        buildLayout()
        seedInitialTrace()
        updateControls()
    }

    private func configureControls() {
        busControl.selectedSegment = 0
        portPopup.addItems(withTitles: ["Mock Pico /dev/cu.usbmodem1101", "Manual serial port"])
        baudPopup.addItems(withTitles: ["115200", "230400", "460800", "921600", "1000000"])
        baudPopup.selectItem(withTitle: "115200")
        i2cSpeedPopup.addItems(withTitles: ["100 kHz", "400 kHz", "1 MHz"])
        i2cSpeedPopup.selectItem(withTitle: "400 kHz")
        attackPopup.addItems(withTitles: ["Address sweep", "Malformed length", "Repeated START", "Random bytes"])
        modePopup.addItems(withTitles: ["Sequential", "Random", "Corpus-guided"])

        frequencySlider.target = self
        frequencySlider.action = #selector(rateChanged)

        connectButton.target = self
        connectButton.action = #selector(connectTapped)
        capsButton.target = self
        capsButton.action = #selector(capsTapped)
        armButton.target = self
        armButton.action = #selector(armTapped)
        captureButton.target = self
        captureButton.action = #selector(captureTapped)
        stopButton.target = self
        stopButton.action = #selector(stopTapped)
        disarmButton.target = self
        disarmButton.action = #selector(disarmTapped)
        queueButton.target = self
        queueButton.action = #selector(queueTapped)
        fuzzButton.target = self
        fuzzButton.action = #selector(fuzzTapped)

        [connectButton, capsButton, armButton, captureButton, stopButton, disarmButton, queueButton, fuzzButton].forEach {
            $0.bezelStyle = .rounded
            $0.controlSize = .regular
        }

        statusLabel.font = NSFont.systemFont(ofSize: 13, weight: .semibold)
        sessionLabel.font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .regular)
        countersLabel.font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .regular)
        timelineLabel.font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .medium)

        setupTable()
    }

    private func setupTable() {
        tableView.dataSource = dataSource
        tableView.delegate = dataSource
        tableView.headerView = NSTableHeaderView()
        tableView.usesAlternatingRowBackgroundColors = true
        tableView.rowHeight = 28
        tableView.gridStyleMask = [.solidHorizontalGridLineMask]

        [
            ("timestamp", "Time", 115),
            ("bus", "Bus", 70),
            ("event", "Event", 95),
            ("data", "Data", 245),
            ("note", "Decoded", 320)
        ].forEach { id, title, width in
            let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(id))
            column.title = title
            column.width = CGFloat(width)
            column.minWidth = CGFloat(max(60, width - 70))
            tableView.addTableColumn(column)
        }
    }

    private func buildLayout() {
        let root = NSStackView()
        root.orientation = .vertical
        root.spacing = 12
        root.edgeInsets = NSEdgeInsets(top: 16, left: 16, bottom: 16, right: 16)
        root.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(root)

        let header = makeHeader()
        let split = NSSplitView()
        split.isVertical = true
        split.dividerStyle = .thin
        split.translatesAutoresizingMaskIntoConstraints = false

        let left = makeConnectionPanel()
        let center = makeTracePanel()
        let right = makeFuzzerPanel()
        split.addArrangedSubview(left)
        split.addArrangedSubview(center)
        split.addArrangedSubview(right)

        root.addArrangedSubview(header)
        root.addArrangedSubview(split)

        NSLayoutConstraint.activate([
            root.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            root.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            root.topAnchor.constraint(equalTo: view.topAnchor),
            root.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            header.heightAnchor.constraint(equalToConstant: 54),
            left.widthAnchor.constraint(equalToConstant: 310),
            right.widthAnchor.constraint(equalToConstant: 300)
        ])
    }

    private func makeHeader() -> NSView {
        let container = NSView()
        container.wantsLayer = true
        container.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        container.layer?.cornerRadius = 8

        let title = NSTextField(labelWithString: "Hardware Protocol Fuzzer")
        title.font = NSFont.systemFont(ofSize: 20, weight: .bold)

        let subtitle = NSTextField(labelWithString: "Desktop prototype: konfiguracja I2C/UART, podglad ramek i panel fuzzera")
        subtitle.font = NSFont.systemFont(ofSize: 12)
        subtitle.textColor = .secondaryLabelColor

        let titleStack = NSStackView(views: [title, subtitle])
        titleStack.orientation = .vertical
        titleStack.spacing = 2
        titleStack.translatesAutoresizingMaskIntoConstraints = false

        let statusStack = NSStackView(views: [statusLabel, sessionLabel])
        statusStack.orientation = .vertical
        statusStack.alignment = .trailing
        statusStack.spacing = 4
        statusStack.translatesAutoresizingMaskIntoConstraints = false

        container.addSubview(titleStack)
        container.addSubview(statusStack)

        NSLayoutConstraint.activate([
            titleStack.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 16),
            titleStack.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            statusStack.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -16),
            statusStack.centerYAnchor.constraint(equalTo: container.centerYAnchor),
            titleStack.trailingAnchor.constraint(lessThanOrEqualTo: statusStack.leadingAnchor, constant: -20)
        ])

        return container
    }

    private func makeConnectionPanel() -> NSView {
        let stack = panelStack(title: "Connection")
        stack.addArrangedSubview(row("Protocol", busControl))
        stack.addArrangedSubview(row("Port", portPopup))
        stack.addArrangedSubview(row("UART baud", baudPopup))
        stack.addArrangedSubview(row("I2C speed", i2cSpeedPopup))
        stack.addArrangedSubview(row("I2C address", addressField))
        stack.addArrangedSubview(row("SDA / TX pin", sdaField))
        stack.addArrangedSubview(row("SCL / RX pin", sclField))
        stack.addArrangedSubview(buttonGrid([connectButton, capsButton, armButton, captureButton, stopButton, disarmButton]))
        stack.addArrangedSubview(separator())
        stack.addArrangedSubview(infoLine("Protocol frame", "16 B header + payload, CRC-16/CCITT-FALSE"))
        stack.addArrangedSubview(infoLine("Safety", "No active pins before ARM"))
        stack.addArrangedSubview(infoLine("Mock mode", "Generates demo TRACE_DECODED frames"))
        return wrap(stack)
    }

    private func makeTracePanel() -> NSView {
        let stack = panelStack(title: "Captured Frames")

        let toolbar = NSStackView()
        toolbar.orientation = .horizontal
        toolbar.spacing = 8
        toolbar.alignment = .centerY

        let clearButton = NSButton(title: "Clear", target: self, action: #selector(clearTapped))
        clearButton.bezelStyle = .rounded
        let addDemoButton = NSButton(title: "Add Demo Frame", target: self, action: #selector(addDemoTapped))
        addDemoButton.bezelStyle = .rounded
        let spacer = NSView()

        toolbar.addArrangedSubview(timelineLabel)
        toolbar.addArrangedSubview(spacer)
        toolbar.addArrangedSubview(addDemoButton)
        toolbar.addArrangedSubview(clearButton)
        spacer.setContentHuggingPriority(.defaultLow, for: .horizontal)

        let scrollView = NSScrollView()
        scrollView.documentView = tableView
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = true
        scrollView.borderType = .lineBorder
        scrollView.translatesAutoresizingMaskIntoConstraints = false

        stack.addArrangedSubview(toolbar)
        stack.addArrangedSubview(scrollView)
        stack.addArrangedSubview(countersLabel)

        NSLayoutConstraint.activate([
            scrollView.heightAnchor.constraint(greaterThanOrEqualToConstant: 380)
        ])

        return wrap(stack)
    }

    private func makeFuzzerPanel() -> NSView {
        let stack = panelStack(title: "Fuzzer Control")
        stack.addArrangedSubview(row("Attack", attackPopup))
        stack.addArrangedSubview(row("Selection", modePopup))
        stack.addArrangedSubview(row("Frequency", frequencySlider))
        stack.addArrangedSubview(row("Rate", rateLabel))
        stack.addArrangedSubview(row("Repeats", repeatField))
        stack.addArrangedSubview(row("Stimulus", stimulusField))
        stack.addArrangedSubview(buttonGrid([queueButton, fuzzButton]))
        stack.addArrangedSubview(separator())
        stack.addArrangedSubview(infoLine("Policy", "max_pending <= 32, pending_bytes <= 4096"))
        stack.addArrangedSubview(infoLine("Sequence", "SET_FUZZ_POLICY -> QUEUE_STIMULUS -> ARM -> START_FUZZ"))
        stack.addArrangedSubview(infoLine("Correlation", "stimulus_id links FUZZ_TX with TRACE_DECODED"))
        return wrap(stack)
    }

    private func panelStack(title: String) -> NSStackView {
        let titleLabel = NSTextField(labelWithString: title)
        titleLabel.font = NSFont.systemFont(ofSize: 15, weight: .semibold)

        let stack = NSStackView(views: [titleLabel])
        stack.orientation = .vertical
        stack.spacing = 10
        stack.alignment = .leading
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }

    private func wrap(_ stack: NSStackView) -> NSView {
        let container = NSView()
        container.wantsLayer = true
        container.layer?.backgroundColor = NSColor.textBackgroundColor.cgColor
        container.layer?.cornerRadius = 8
        container.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(stack)

        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 14),
            stack.trailingAnchor.constraint(equalTo: container.trailingAnchor, constant: -14),
            stack.topAnchor.constraint(equalTo: container.topAnchor, constant: 14),
            stack.bottomAnchor.constraint(equalTo: container.bottomAnchor, constant: -14)
        ])

        return container
    }

    private func row(_ label: String, _ control: NSView) -> NSView {
        let labelView = NSTextField(labelWithString: label)
        labelView.font = NSFont.systemFont(ofSize: 12, weight: .medium)
        labelView.textColor = .secondaryLabelColor
        labelView.widthAnchor.constraint(equalToConstant: 88).isActive = true

        control.translatesAutoresizingMaskIntoConstraints = false
        control.widthAnchor.constraint(greaterThanOrEqualToConstant: 150).isActive = true

        let stack = NSStackView(views: [labelView, control])
        stack.orientation = .horizontal
        stack.spacing = 8
        stack.alignment = .centerY
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }

    private func buttonGrid(_ buttons: [NSButton]) -> NSView {
        let grid = NSGridView()
        grid.translatesAutoresizingMaskIntoConstraints = false
        grid.rowSpacing = 8
        grid.columnSpacing = 8

        for index in stride(from: 0, to: buttons.count, by: 2) {
            let first = buttons[index]
            let second = index + 1 < buttons.count ? buttons[index + 1] : NSView()
            grid.addRow(with: [first, second])
            first.widthAnchor.constraint(equalToConstant: 132).isActive = true
            second.widthAnchor.constraint(equalToConstant: 132).isActive = true
        }
        return grid
    }

    private func separator() -> NSView {
        let line = NSBox()
        line.boxType = .separator
        line.translatesAutoresizingMaskIntoConstraints = false
        return line
    }

    private func infoLine(_ title: String, _ value: String) -> NSView {
        let titleView = NSTextField(labelWithString: title)
        titleView.font = NSFont.systemFont(ofSize: 12, weight: .semibold)
        let valueView = NSTextField(labelWithString: value)
        valueView.font = NSFont.systemFont(ofSize: 12)
        valueView.textColor = .secondaryLabelColor
        valueView.lineBreakMode = .byWordWrapping
        valueView.maximumNumberOfLines = 3

        let stack = NSStackView(views: [titleView, valueView])
        stack.orientation = .vertical
        stack.spacing = 2
        stack.alignment = .leading
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }

    private func seedInitialTrace() {
        append(record: TraceRecord(timestamp: "00:00.000", bus: "I2C", event: "START", data: "", note: "Bus idle -> transaction"))
        append(record: TraceRecord(timestamp: "00:00.014", bus: "I2C", event: "BYTE", data: "0x90", note: "Address 0x48 write"))
        append(record: TraceRecord(timestamp: "00:00.021", bus: "I2C", event: "ACK", data: "", note: "Target acknowledged"))
        append(record: TraceRecord(timestamp: "00:00.036", bus: "I2C", event: "BYTE", data: "0x00 0x7F", note: "Register payload"))
        append(record: TraceRecord(timestamp: "00:00.051", bus: "I2C", event: "STOP", data: "", note: "Transaction complete"))
    }

    private func append(record: TraceRecord) {
        dataSource.records.append(record)
        tableView.reloadData()
        if dataSource.records.count > 0 {
            tableView.scrollRowToVisible(dataSource.records.count - 1)
        }
        timelineLabel.stringValue = "\(dataSource.records.count) frames"
    }

    private func updateControls() {
        sessionLabel.stringValue = "Session: 0x\(String(format: "%04X", sessionID))"
        countersLabel.stringValue = "RX overruns: \(rxOverruns)   TX underruns: \(txUnderruns)   queued: \(queuedStimuli)"

        switch deviceState {
        case .detached:
            statusLabel.stringValue = "Detached"
            statusLabel.textColor = .secondaryLabelColor
        case .connected:
            statusLabel.stringValue = "Connected"
            statusLabel.textColor = .systemBlue
        case .capabilitiesRead:
            statusLabel.stringValue = "Capabilities read"
            statusLabel.textColor = .systemBlue
        case .armed:
            statusLabel.stringValue = queuedStimuli > 0 ? "Armed, stimuli queued" : "Armed mock session"
            statusLabel.textColor = queuedStimuli > 0 ? .systemOrange : .systemBlue
        case .capturing:
            statusLabel.stringValue = "Capturing"
            statusLabel.textColor = .systemGreen
        case .fuzzing:
            statusLabel.stringValue = "Running fuzz"
            statusLabel.textColor = .systemPurple
        case .disarmed:
            statusLabel.stringValue = "Disarmed"
            statusLabel.textColor = .secondaryLabelColor
        }
    }

    private func startTimer() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.75, repeats: true) { [weak self] _ in
            self?.appendGeneratedFrame()
        }
    }

    private func stopTimer() {
        timer?.invalidate()
        timer = nil
    }

    private func appendGeneratedFrame() {
        frameSequence += 1
        let time = String(format: "00:%02d.%03d", (frameSequence / 2) % 60, (frameSequence * 137) % 1000)
        let bus = busControl.selectedSegment == 0 ? "I2C" : "UART"

        if fuzzRunning {
            let stimulus = String(format: "id=%04X  %@", frameSequence, stimulusField.stringValue)
            append(record: TraceRecord(timestamp: time, bus: bus, event: "FUZZ_TX", data: stimulus, note: attackPopup.titleOfSelectedItem ?? "Stimulus sent"))
            if frameSequence % 4 == 0 {
                append(record: TraceRecord(timestamp: time, bus: bus, event: "NACK", data: "", note: "Target rejected mutated frame"))
            }
        } else {
            let events = ["START", "BYTE", "ACK", "BYTE", "STOP"]
            let event = events[frameSequence % events.count]
            let payload = event == "BYTE" ? String(format: "0x%02X 0x%02X", (0x40 + frameSequence) & 0xFF, (0xA0 + frameSequence * 3) & 0xFF) : ""
            let note = event == "BYTE" ? "Decoded payload chunk" : "Protocol marker"
            append(record: TraceRecord(timestamp: time, bus: bus, event: event, data: payload, note: note))
        }
    }

    @objc private func connectTapped() {
        deviceState = .connected
        append(record: TraceRecord(timestamp: "00:00.100", bus: "USB", event: "HELLO_ACK", data: "protocol=1 session=0x0042", note: "Mock Pico connected"))
        updateControls()
    }

    @objc private func capsTapped() {
        deviceState = .capabilitiesRead
        append(record: TraceRecord(timestamp: "00:00.140", bus: "USB", event: "CAPS", data: "I2C UART PIO=8 BUF=128KiB", note: "Capabilities received"))
        updateControls()
    }

    @objc private func armTapped() {
        captureRunning = false
        fuzzRunning = false
        deviceState = .armed
        stopTimer()
        append(record: TraceRecord(timestamp: "00:00.180", bus: "USB", event: "ARM_OK", data: "state=ARMED", note: "Configuration validated"))
        updateControls()
    }

    @objc private func captureTapped() {
        captureRunning = true
        fuzzRunning = false
        deviceState = .capturing
        append(record: TraceRecord(timestamp: "00:00.200", bus: "USB", event: "START", data: "START_CAPTURE", note: "Live capture mock started"))
        updateControls()
        startTimer()
    }

    @objc private func stopTapped() {
        captureRunning = false
        fuzzRunning = false
        deviceState = .armed
        stopTimer()
        append(record: TraceRecord(timestamp: "00:00.260", bus: "USB", event: "STOP_OK", data: "drained=512", note: "Buffers drained, back to armed"))
        updateControls()
    }

    @objc private func disarmTapped() {
        captureRunning = false
        fuzzRunning = false
        queuedStimuli = 0
        deviceState = .disarmed
        stopTimer()
        append(record: TraceRecord(timestamp: "00:00.300", bus: "USB", event: "DISARM", data: "pins=HIGH-Z", note: "Safe state"))
        updateControls()
    }

    @objc private func queueTapped() {
        queuedStimuli = min(32, queuedStimuli + 1)
        if deviceState != .fuzzing && deviceState != .capturing {
            deviceState = .armed
        }
        append(record: TraceRecord(timestamp: "00:00.320", bus: "USB", event: "QUEUE", data: stimulusField.stringValue, note: attackPopup.titleOfSelectedItem ?? "Stimulus queued"))
        updateControls()
    }

    @objc private func fuzzTapped() {
        if queuedStimuli == 0 {
            queueTapped()
        }
        captureRunning = false
        fuzzRunning = true
        deviceState = .fuzzing
        append(record: TraceRecord(timestamp: "00:00.350", bus: "USB", event: "START", data: "START_FUZZ", note: "Fuzzer scheduler mock started"))
        updateControls()
        startTimer()
    }

    @objc private func clearTapped() {
        dataSource.records.removeAll()
        tableView.reloadData()
        timelineLabel.stringValue = "0 frames"
    }

    @objc private func addDemoTapped() {
        appendGeneratedFrame()
    }

    @objc private func rateChanged() {
        rateLabel.stringValue = "\(Int(frequencySlider.doubleValue)) Hz"
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var window: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        let controller = MainViewController()
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1220, height: 760),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Hardware Protocol Fuzzer - Desktop Prototype"
        window.center()
        window.contentViewController = controller
        window.makeKeyAndOrderFront(nil)
        self.window = window
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.activate(ignoringOtherApps: true)
app.run()
