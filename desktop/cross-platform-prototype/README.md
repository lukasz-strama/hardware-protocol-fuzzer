# Cross-Platform Desktop Frontend Prototype

Statyczny prototyp frontendowy, dzialajacy w przegladarce na Windows, Linux i macOS. Nie wymaga instalacji Qt, Node, Pythona ani firmware na Raspberry Pi Pico.

## Uruchomienie

Otworz plik:

```text
desktop/cross-platform-prototype/index.html
```

Mozesz tez uruchomic lokalny serwer HTTP, jesli wygodniej pokazac adres URL:

```sh
cd desktop/cross-platform-prototype
python3 -m http.server 8080
```

Nastepnie wejdz w przegladarce na:

```text
http://localhost:8080
```

## Szybka sciezka demo

1. Kliknij `Connect`.
2. Kliknij `Get Caps`.
3. Ustaw `Protocol`, piny i parametry I2C/UART.
4. Kliknij `Arm`.
5. Kliknij `Start Capture`, zeby zobaczyc dopisywane ramki.
6. Kliknij `Queue Stimulus`, a potem `Start Fuzz`, zeby pokazac fuzzer.
7. Kliknij `Stop` albo `Disarm`, zeby pokazac bezpieczny stan.

## Zakres

Ten prototyp pokazuje Twoja czesc projektu: okna/panele konfiguracji polaczenia, wizualizacje przechwyconych ramek i panel sterowania fuzzerem. Transport USB i firmware sa mockowane.
