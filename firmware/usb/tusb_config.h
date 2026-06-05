/**
 * @file tusb_config.h
 * @brief Konfiguracja TinyUSB dla urządzenia RP2040 (CDC).
 *
 * Plik definiuje:
 * - typ MCU,
 * - tryb pracy USB (device, full-speed),
 * - aktywowane klasy (CDC),
 * - rozmiary endpointów i buforów.
 *
 * Plik jest wczytywany przez TinyUSB podczas kompilacji.
 */
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_MCU          OPT_MCU_RP2040
#define CFG_TUSB_OS           OPT_OS_PICO

/** Port root-hub 0 działa jako urządzenie USB FS. */
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

/** Włączone klasy USB. */
#define CFG_TUD_ENABLED       1
#define CFG_TUD_CDC           1
#define CFG_TUD_VENDOR        0    

/** Rozmiary endpointów i buforów CDC. */
#define CFG_TUD_ENDPOINT0_SIZE  64
#define CFG_TUD_CDC_RX_BUFSIZE  256
#define CFG_TUD_CDC_TX_BUFSIZE  512
#define CFG_TUD_CDC_EP_BUFSIZE  64

#endif