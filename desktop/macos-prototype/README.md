# Desktop Prototype

Natywny prototyp macOS dla czesci desktopowej projektu. Pokazuje trzy elementy wymagane na prezentacje:

- konfiguracje polaczenia I2C/UART,
- wizualizacje przechwyconych ramek w przewijanej tabeli,
- panel sterowania fuzzerem z wyborem ataku, trybu i czestotliwosci.

Prototyp dziala w trybie mock, bez podlaczonego Raspberry Pi Pico. Przyciski dopisuja do tabeli przykładowe rekordy zgodne z kontraktem projektu: `HELLO_ACK`, `CAPS`, `ARM_OK`, `TRACE_DECODED`, `FUZZ_TX`, `STOP_OK`.

## Uruchomienie

```sh
cd desktop/macos-prototype
make run
```

## Szybka sciezka demo

1. Kliknij `Connect`.
2. Kliknij `Get Caps`.
3. Wybierz parametry I2C albo UART.
4. Kliknij `Arm`.
5. Kliknij `Start Capture`, zeby zobaczyc dopisywane ramki.
6. Kliknij `Queue Stimulus`, a potem `Start Fuzz`, zeby pokazac panel fuzzera.
7. Kliknij `Stop` albo `Disarm`, zeby wrocic do bezpiecznego stanu.
