# Qt Desktop Frontend Prototype

Glowny prototyp desktopowej czesci projektu. Aplikacja jest napisana w C++/Qt Widgets i pokazuje:

- konfiguracje polaczenia I2C/UART,
- przewijana tabele przechwyconych ramek,
- panel fuzzera z wyborem typu ataku, trybu, bodzcow i czestotliwosci,
- mockowany przebieg `HELLO_ACK -> CAPS -> ARM_OK -> START_CAPTURE/START_FUZZ -> STOP_OK/DISARM`.

Transport USB i firmware sa na razie mockowane. UI jest przygotowane tak, zeby pozniej podpiac realny transport i parser ramek z `docs/protocol_layout.h`.

## Wymagania

- Qt 6 z modulem Widgets,
- CMake 3.16+ albo qmake,
- kompilator C++17.

Na macOS z Homebrew:

```sh
brew install qt cmake
```

## Build przez CMake

```sh
cd desktop/qt-frontend-prototype
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
```

Uruchomienie na macOS:

```sh
open build/HardwareProtocolFuzzerDesktop.app
```

Na Windows/Linux sciezka do binarki bedzie w katalogu `build` wygenerowanym przez CMake.

## Build przez qmake

```sh
cd desktop/qt-frontend-prototype
qmake qt-frontend-prototype.pro
make
```

## Szybka sciezka demo

1. Kliknij `Connect`.
2. Kliknij `Get Caps`.
3. Ustaw parametry I2C albo UART.
4. Kliknij `Arm`.
5. Kliknij `Start Capture`, zeby zobaczyc dopisywane ramki.
6. Kliknij `Queue Stimulus`, a potem `Start Fuzz`.
7. Kliknij `Stop` albo `Disarm`.
