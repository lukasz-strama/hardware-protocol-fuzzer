/**
 * @file usb_descriptors.c
 * @brief Deskryptory USB oraz callbacki TinyUSB dla urządzenia CDC.
 *
 * Ten moduł dostarcza:
 * - deskryptor urządzenia (Device Descriptor),
 * - deskryptor konfiguracji (Configuration Descriptor),
 * - deskryptory stringów (String Descriptors),
 * - callbacki wymagane przez TinyUSB do enumeracji.
 *
 * Urządzenie zgłasza się jako:
 * - klasa MISC z IAD,
 * - interfejs CDC (notification + data),
 * - jedna konfiguracja USB FS.
 */
#include "tusb.h"
#include <stdint.h>
#include <string.h>
#include "usb_transport.h"

/**
 * @brief Standardowy deskryptor urządzenia USB.
 *
 * Zawiera podstawowe informacje o urządzeniu:
 * - wersja USB 2.0,
 * - klasa MISC (IAD),
 * - identyfikatory VID/PID,
 * - indeksy stringów producenta, produktu i numeru seryjnego.
 */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

/**
 * @brief Callback TinyUSB zwracający deskryptor urządzenia.
 *
 * Wywoływany podczas enumeracji USB.
 */
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

// Endpointy CDC
#define EPNUM_CDC_NOTIF  0x81
#define EPNUM_CDC_OUT    0x02
#define EPNUM_CDC_IN     0x82

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)


/**
 * @brief Deskryptor konfiguracji USB FS.
 *
 * Zawiera:
 * - konfigurację,
 * - interfejs CDC (notification + data),
 * - endpointy IN/OUT.
 */
uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

/**
 * @brief Callback TinyUSB zwracający deskryptor konfiguracji.
 */
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_fs_configuration;
}

/**
 * @brief Tablica deskryptorów stringów USB.
 *
 * Indexy:
 * 0 - język (0x0409)  
 * 1 - producent  
 * 2 - produkt  
 * 3 - numer seryjny  
 * 4 - opis interfejsu CDC  
 */
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "PicoSniffer",               
    "I2C/UART Sniffer",         
    "123456",                   
    "CDC Data",                  
};

static uint16_t _desc_str[32];

/**
 * @brief Callback TinyUSB zwracający deskryptor stringu.
 *
 * Konwertuje ASCII - UTF‑16LE zgodnie z wymaganiami USB.
 *
 * @param index Numer stringu.
 * @param langid Ignorowane.
 * @return Wskaźnik na bufor UTF‑16LE lub NULL jeśli index niepoprawny.
 */
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2*chr_count + 2));
    return _desc_str;
}