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

## Jak obslugiwac ekran

Aplikacja dziala teraz w trybie `MOCK MODE`, czyli nie laczy sie jeszcze z prawdziwym Raspberry Pi Pico. Dane w tabeli sa symulowane lokalnie, zeby pokazac zachowanie frontendu przed gotowym firmware.

Panel `Connection` sluzy do ustawienia parametrow polaczenia:

- `Protocol`: wybor `I2C` albo `UART`,
- `Port`: docelowo port urzadzenia, teraz mock,
- `UART baud` i `Parity`: parametry UART,
- `I2C speed` i `I2C addr`: parametry I2C,
- `SDA / TX`, `SCL / RX`: piny magistrali,
- `Pull-up` i `Vtarget`: ustawienia celu,
- `Session log`: log komend sesji, np. `HELLO`, `GET_CAPS`, `ARM`, `STOP`.

Panel `Captured Frames` pokazuje symulowane rekordy trace. Kolumny oznaczaja:

- `Seq`: numer rekordu,
- `Time`: znacznik czasu,
- `Bus`: `I2C` albo `UART`,
- `Event`: typ zdarzenia, np. `BYTE`, `START`, `STOP`, `ACK`, `NACK`, `BREAK`, `FUZZ_TX`, `OVERFLOW`,
- `Len`: liczba bajtow danych,
- `Data`: dane w hex,
- `Decoded`: opis zdekodowanego zdarzenia.

Panel `Fuzzer Control` sluzy do ustawienia mockowanej polityki fuzzera:

- `Attack`: typ scenariusza,
- `Selection`: sposob wyboru bodzcow,
- `Stimulus`: bajty bodzca,
- `Repeats`, `Budget`, `Frequency`: podstawowe limity wykonania,
- `Queue Stimulus`: dodaje bodziec do kolejki,
- `Start Fuzz`: uruchamia symulowany fuzzing.

## Blokowanie przyciskow wedlug stanu

- Na poczatku dziala tylko `Connect`.
- Po `Connect` dziala `Get Caps`.
- Po `Get Caps` dziala `Arm`.
- Po `Arm` dzialaja `Start Capture`, `Queue Stimulus`, `Start Fuzz`, `Disarm`.
- Podczas capture/fuzz dziala glownie `Stop`, a konfiguracja jest zablokowana.
