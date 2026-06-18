# Qt Desktop Frontend

Desktopowa czesc projektu. Aplikacja jest napisana w C++/Qt Widgets i łączy się z firmware Pico przez USB CDC/serial, korzystając z protokołu zdefiniowanego w `docs/contract.md` i współdzielonych struktur z repozytorium root.

UI pokazuje:

- konfigurację portu szeregowego i parametrów UART,
- przewijaną tabelę przechwyconych ramek,
- panel logów sesji,
- podstawowe sterowanie capture oraz miejsce na fuzzing, gdy firmware zacznie go wystawiać.

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

Uruchomienie na Linuxie:

```sh
./build/HardwareProtocolFuzzerDesktop
```

Jeśli chcesz wskazać inny port, wybierz go w polu `Port` w aplikacji przed kliknięciem `Connect`.

## Build przez qmake

```sh
cd desktop/qt-frontend-prototype
qmake qt-frontend-prototype.pro
make
```

## Szybka sciezka demo

1. Wgraj firmware na Pico i podłącz je do komputera przez USB.
2. Wybierz port, zwykle `/dev/ttyACM0` albo `/dev/ttyUSB0`.
3. Kliknij `Connect`.
4. Kliknij `Get Caps`.
5. Kliknij `Arm`.
6. Kliknij `Start Capture`, żeby zobaczyć live `TRACE_DECODED`.
7. Kliknij `Stop`, a potem `Disarm`.

## Jak obslugiwac ekran

Aplikacja działa z rzeczywistym backendem serialowym. Jeśli firmware nie odpowiada, aplikacja pokaże błąd po `Connect` lub `Get Caps`.

Panel `Connection` sluży do ustawienia parametrów połączenia:

- `Protocol`: obecnie UART,
- `Port`: port urządzenia Pico,
- `UART baud` i `Parity`: parametry UART,
- `TX` i `RX`: piny używane przez sniffer,
- `Vtarget`: ustawienie celu,
- `Session log`: log komend sesji, np. `HELLO`, `GET_CAPS`, `ARM`, `STOP`.

Panel `Captured Frames` pokazuje live rekordy trace z firmware. Kolumny oznaczaja:

- `Seq`: numer rekordu,
- `Time`: znacznik czasu,
- `Bus`: `I2C` albo `UART`,
- `Event`: typ zdarzenia, np. `BYTE`, `START`, `STOP`, `ACK`, `NACK`, `BREAK`, `FUZZ_TX`, `OVERFLOW`,
- `Len`: liczba bajtow danych,
- `Data`: dane w hex,
- `Decoded`: opis zdekodowanego zdarzenia.

Panel `Fuzzing` jest teraz celowo prosty i opisuje tylko to, co desktop faktycznie obsługuje:

- `Mode`: `Sequential once` albo `Random repeat`,
- `Budget ms`: limit czasu sesji,
- `Stimulus`: źródłowy ciąg hex dla korpusu,
- `Bit flip` i `Truncate`: jedyne dwie mutacje wystawione w UI,
- `Run Fuzz` / `Stop Fuzz`: uruchomienie i zatrzymanie sesji,
- `Fuzz Results`: wynikowe dopasowanie `FUZZ_TX` do odpowiedzi.

UI nie pokazuje już pseudo-opcji dla I2C fuzzingu. Desktop uruchamia obecnie tylko UART-owy fuzzing, a capture I2C/UART zostaje bez zmian w panelu przechwytywania.

## Blokowanie przyciskow wedlug stanu

- Na poczatku dziala tylko `Connect`.
- Po `Connect` dziala `Get Caps`.
- Po `Get Caps` dziala `Arm`.
- Po `Arm` działają `Start Capture` i `Disarm`.
- Podczas capture/fuzz dziala glownie `Stop`, a konfiguracja jest zablokowana.
