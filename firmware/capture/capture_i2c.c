#include "capture_i2c.h"
#include "trace_encoder.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "capture_i2c.pio.h"

#include <assert.h>

#ifndef PIO_FDEBUG_RXOVER_LSB
#define PIO_FDEBUG_RXOVER_LSB 8
#endif

static PIO i2c_pio = pio1;

static uint sm_data;
static uint sm_cond;

static uint offset_data;
static uint offset_cond;

static uint8_t pin_sda;
static uint8_t pin_scl;

void on_i2c_pio_irq(void)
{
    if (pio_interrupt_get(i2c_pio, 0))
    {
        pio_interrupt_clear(i2c_pio, 0);

        trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_START, NULL, 0);
    }

    if (pio_interrupt_get(i2c_pio, 1))
    {
        pio_interrupt_clear(i2c_pio, 1);

        trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_STOP, NULL, 0);
    }
}

void capture_i2c_init(uint8_t sda, uint8_t scl)
{
    assert(scl == (sda + 1));

    pin_sda = sda;
    pin_scl = scl;

    pio_gpio_init(i2c_pio, sda);
    pio_gpio_init(i2c_pio, scl);

    gpio_set_dir(sda, GPIO_IN);
    gpio_set_dir(scl, GPIO_IN);

    gpio_pull_up(sda);
    gpio_pull_up(scl);

    offset_data = pio_add_program(i2c_pio, &i2c_data_program);

    sm_data = pio_claim_unused_sm(i2c_pio, true);

    {
        pio_sm_config c = i2c_data_program_get_default_config(offset_data);

        sm_config_set_in_pins(&c, sda);
        sm_config_set_jmp_pin(&c, scl);

        sm_config_set_clkdiv(&c, 1.0f);

        sm_config_set_in_shift(&c, false, false, 32);

        pio_sm_init(i2c_pio, sm_data, offset_data, &c);
    }

    offset_cond = pio_add_program(i2c_pio, &i2c_conditions_program);

    sm_cond = pio_claim_unused_sm(i2c_pio, true);

    {
        pio_sm_config c = i2c_conditions_program_get_default_config(offset_cond);

        sm_config_set_in_pins(&c, sda);
        sm_config_set_jmp_pin(&c, sda);

        sm_config_set_clkdiv(&c, 1.0f);

        pio_sm_init(i2c_pio, sm_cond, offset_cond, &c);
    }

    pio_set_irq0_source_enabled(i2c_pio, pis_interrupt0, true);

    pio_set_irq0_source_enabled(i2c_pio, pis_interrupt1, true);

    int pio_irq = (i2c_pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;

    irq_set_exclusive_handler(pio_irq, on_i2c_pio_irq);

    irq_set_enabled(pio_irq, true);
}

void capture_i2c_start(void)
{
    pio_sm_set_enabled(i2c_pio, sm_cond, true);

    pio_sm_set_enabled(i2c_pio, sm_data, true);
}

void capture_i2c_poll(void)
{
    while (!pio_sm_is_rx_fifo_empty(i2c_pio, sm_data))
    {
        uint32_t raw = pio_sm_get(i2c_pio, sm_data);

        uint8_t data = (uint8_t)(raw >> 24);

        uint8_t ack = (uint8_t)((raw >> 23) & 1);

        uint32_t ts = time_us_32();

        trace_emit(ts, TRACE_SOURCE_I2C, TRACE_EVENT_BYTE, &data, 1);

        trace_emit(ts, TRACE_SOURCE_I2C, ack ? TRACE_EVENT_NACK : TRACE_EVENT_ACK, NULL, 0);
    }

    if (i2c_pio->fdebug &
        (1u << (PIO_FDEBUG_RXOVER_LSB + sm_data)))
    {
        i2c_pio->fdebug =
            (1u << (PIO_FDEBUG_RXOVER_LSB + sm_data));

        trace_emit(time_us_32(), TRACE_SOURCE_I2C, TRACE_EVENT_OVERFLOW, NULL, 0);
    }
}

void capture_i2c_stop(void)
{
    pio_sm_set_enabled(i2c_pio, sm_data, false);

    pio_sm_set_enabled(i2c_pio, sm_cond, false);

    int pio_irq = (i2c_pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;

    irq_set_enabled(pio_irq, false);

    pio_interrupt_clear(i2c_pio, 0);

    pio_interrupt_clear(i2c_pio, 1);

    irq_remove_handler(pio_irq, on_i2c_pio_irq);
    pio_sm_clear_fifos(i2c_pio, sm_data);
    pio_remove_program(i2c_pio, &i2c_data_program, offset_data);

    pio_remove_program(i2c_pio, &i2c_conditions_program, offset_cond);

    pio_sm_unclaim(i2c_pio, sm_data);

    pio_sm_unclaim(i2c_pio, sm_cond);
}