// #include "capture_i2c.h"
// #include "trace_encoder.h"
// #include "hardware/pio.h"
// #include "hardware/irq.h"
// #include "hardware/clocks.h"
// #include "pico/stdlib.h"
// #include "pico/time.h"
// #include "capture_i2c.pio.h"

// #ifndef PIO_FDEBUG_RXOVER_LSB
// #define PIO_FDEBUG_RXOVER_LSB 8
// #endif

// static PIO  i2c_pio = pio1;
// static uint sm_i2c;
// static uint offset_i2c;

// static uint8_t pin_sda;
// static uint8_t pin_scl;

// static void on_i2c_pio_irq(void) {
//     if (pio_interrupt_get(i2c_pio, 0)) {
//         pio_interrupt_clear(i2c_pio, 0);
//         trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_START, NULL, 0);
//     }
//     if (pio_interrupt_get(i2c_pio, 1)) {
//         pio_interrupt_clear(i2c_pio, 1);
//         trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_STOP, NULL, 0);
//     }
// }

// void capture_i2c_init(uint8_t sda, uint8_t scl) {
//     pin_sda = sda;
//     pin_scl = scl;

//     offset_i2c = pio_add_program(i2c_pio, &i2c_sniffer_program);
//     sm_i2c     = pio_claim_unused_sm(i2c_pio, true);

//     pio_gpio_init(i2c_pio, sda);
//     pio_gpio_init(i2c_pio, scl);
//     gpio_set_dir(sda, GPIO_IN);
//     gpio_set_dir(scl, GPIO_IN);

//     pio_sm_config c = i2c_sniffer_program_get_default_config(offset_i2c);
//     sm_config_set_in_pins(&c, sda);
//     sm_config_set_jmp_pin(&c, scl);
//     sm_config_set_clkdiv(&c, 1.0f);

//     pio_sm_init(i2c_pio, sm_i2c, offset_i2c, &c);

//     pio_set_irq0_source_enabled(i2c_pio, pis_interrupt0, true);
//     pio_set_irq0_source_enabled(i2c_pio, pis_interrupt1, true);

//     int pio_irq = (i2c_pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
//     irq_set_exclusive_handler(pio_irq, on_i2c_pio_irq);
//     irq_set_enabled(pio_irq, true);

//     pio_sm_set_enabled(i2c_pio, sm_i2c, true);
// }

// void capture_i2c_poll(void) {
//     while (!pio_sm_is_rx_fifo_empty(i2c_pio, sm_i2c)) {
//         uint32_t raw  = pio_sm_get(i2c_pio, sm_i2c);
//         uint8_t  data = (uint8_t)((raw >> 1) & 0xFF);
//         uint8_t  ack  = (uint8_t)(raw & 0x01);

//         trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_BYTE, &data, 1);

//         if (ack == 0) {
//             trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_ACK,  NULL, 0);
//         } else {
//             trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_NACK, NULL, 0);
//         }
//     }

//     if (i2c_pio->fdebug & (1u << (PIO_FDEBUG_RXOVER_LSB + sm_i2c))) {
//         i2c_pio->fdebug = (1u << (PIO_FDEBUG_RXOVER_LSB + sm_i2c));
//         trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_OVERFLOW, NULL, 0);
//     }
// }

// void capture_i2c_start(void) {
//     pio_sm_set_enabled(i2c_pio, sm_i2c, true);


// void capture_i2c_stop(void) {
//     pio_sm_set_enabled(i2c_pio, sm_i2c, false);

//     int pio_irq = (i2c_pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
//     irq_set_enabled(pio_irq, false);
//     pio_remove_program(i2c_pio, &i2c_sniffer_program, offset_i2c);
//     pio_sm_unclaim(i2c_pio, sm_i2c);
// }
