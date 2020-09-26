#include <init.h>
#include <rcc.h>
#include <gpio.h>
#include <afio.h>
#include <stk.h>
#include <usart.h>
#include <misc.h>
#include <stddef.h>
#include <stdbool.h>

#define STOP \
    do {                \
        asm ("wfi");    \
    } while (1)

/** A pin definition */
struct pin {
    /** Pin's port */
    volatile struct gpio   *port;
    /** Pin's index on the port */
    unsigned int            idx;
};

#define PIN(_PORT_NAME, _IDX) {.port = GPIO_##_PORT_NAME, .idx = _IDX}

/* Control pin indexes in the pin list */
enum control_pin_idx {
    CONTROL_PIN_IDX_OE,
    CONTROL_PIN_IDX_CE,
    CONTROL_PIN_IDX_WE
};

/* Control pins */
const struct pin control_pin_list[] = {
    [CONTROL_PIN_IDX_OE]    = PIN(B, 0),
    [CONTROL_PIN_IDX_CE]    = PIN(B, 1),
    [CONTROL_PIN_IDX_WE]    = PIN(B, 9),
};
#define oe_pin  (control_pin_list[CONTROL_PIN_IDX_OE])
#define ce_pin  (control_pin_list[CONTROL_PIN_IDX_CE])
#define we_pin  (control_pin_list[CONTROL_PIN_IDX_WE])

/* Address pins (LSB to MSB) */
const struct pin address_pin_list[] = {
    PIN(A, 12),
    PIN(A, 15),
    PIN(B, 3),
    PIN(B, 4),
    PIN(B, 5),
    PIN(B, 6),
    PIN(B, 7),
    PIN(B, 8),
    PIN(A, 4),
    PIN(A, 5),
    PIN(A, 7),
    PIN(A, 6),
    PIN(A, 0),
    PIN(A, 3),
    PIN(A, 2),
    PIN(A, 1),
    PIN(C, 13),
    PIN(C, 14),
    PIN(C, 15),
};

/* Data pins (LSB to MSB) */
const struct pin data_pin_list[] = {
    PIN(A, 11),
    PIN(B, 11),
    PIN(B, 10),
    PIN(B, 12),
    PIN(B, 13),
    PIN(B, 14),
    PIN(B, 15),
    PIN(A, 8),
};
#undef PIN

/**
 * Configure a list of pins with specified mode and configuration.
 *
 * @param ptr   Pointer to the first pin to configure.
 * @param num   Number of pins to configure.
 * @param mode  Pin mode to use.
 * @param conf  Pin configuration to use according to the mode. Either an
 *              enum gpio_cnf_input or an enum gpio_cnf_output value.
 */
void
pin_list_conf(const struct pin *ptr, size_t num,
              enum gpio_mode mode, unsigned int cnf)
{
    for (; num > 0; num--, ptr++) {
        gpio_pin_conf(ptr->port, ptr->idx, mode, cnf);
    }
}

/**
 * Set state of the pins in a list according to a bitmap, stored LSB-first.
 *
 * @param ptr   Pointer to the first pin to set.
 * @param num   Number of pins to set.
 * @param map   The bitmap to get the state from, LSB-first.
 */
void
pin_list_set(const struct pin *ptr, size_t num, unsigned int map)
{
    for (; num > 0; num--, ptr++, map >>= 1) {
        gpio_pin_set(ptr->port, ptr->idx, map & 1);
    }
}

/**
 * Retrieve the state of the pins in a list according,
 * return it as a bitmap, LSB-first.
 *
 * @param ptr   Pointer to the first pin to get.
 * @param num   Number of pins to get.
 */
unsigned int
pin_list_get(const struct pin *ptr, size_t num)
{
    unsigned int map = 0;
    size_t i;
    for (i = 0; i < num; i++, ptr++) {
        map |= gpio_pin_get(ptr->port, ptr->idx) << i;
    }
    return map;
}

volatile struct usart *USART;
volatile unsigned int COUNTER;
#define COUNTER_ADDR    (COUNTER >> 2)
#define COUNTER_PHASE   (COUNTER & 3)
volatile unsigned int BYTE;

void systick_handler(void) __attribute__ ((isr));
void
systick_handler(void)
{
    const char xdigits[] = "0123456789abcdef";
    unsigned int addr = COUNTER_ADDR;
    unsigned int phase = COUNTER_PHASE;

    if (addr == (1 << ARRAY_SIZE(address_pin_list))) {
        return;
    }

    if (phase == 0) {
        /* Set new address each 100us */
        pin_list_set(address_pin_list, ARRAY_SIZE(address_pin_list), addr);
        if ((addr & 0xf) == 0) {
            /* If transmit register is not empty */
            if (!(USART->sr & USART_SR_TXE_MASK)) {
                /* Try again */
                return;
            }
            USART->dr = '\n';
        }
        pin_list_set(&oe_pin, 1, false);
    } else if (phase >= 1) {
        if (phase == 1) {
            BYTE = pin_list_get(data_pin_list, ARRAY_SIZE(data_pin_list));
            pin_list_set(&oe_pin, 1, true);
        }
        /* If transmit register is not empty */
        if (!(USART->sr & USART_SR_TXE_MASK)) {
            /* Try again */
            return;
        }
        if (phase == 3) {
            USART->dr = ((addr & 0xf) == 0xf) ? '\r' : ' ';
        } else {
            USART->dr = xdigits[(phase == 1) ? (BYTE >> 4) : (BYTE & 0xf)];
        }
    }
    COUNTER++;
}

int
main(void)
{
    unsigned int c;

    /* System init */
    init();

    /*
     * Enable clocks to peripherals
     */
    /* Enable APB2 clock to I/O ports and AFIO */
    RCC->apb2enr |= RCC_APB2ENR_AFIOEN_MASK |
                    RCC_APB2ENR_IOPAEN_MASK |
                    RCC_APB2ENR_IOPBEN_MASK |
                    RCC_APB2ENR_IOPCEN_MASK |
                    RCC_APB2ENR_USART1EN_MASK;

    /*
     * Disable JTAG-DP to free up PA15, PB3, and PB4
     */
    AFIO->mapr = (AFIO->mapr & ~AFIO_MAPR_SWJ_CFG_MASK) |
                 (AFIO_MAPR_SWJ_CFG_VAL_JTAG_DP_OFF_SW_DP_ON <<
                  AFIO_MAPR_SWJ_CFG_LSB);

    /*
     * Configure I/O ports
     */
    /* Set control lines high (off) before switching to output */
    pin_list_set(control_pin_list, ARRAY_SIZE(control_pin_list), 0x7);
    /* Configure control lines */
    pin_list_conf(control_pin_list, ARRAY_SIZE(control_pin_list),
                  GPIO_MODE_OUTPUT_2MHZ, GPIO_CNF_OUTPUT_GP_OPEN_DRAIN);
    /* Configure address lines */
    pin_list_conf(address_pin_list, ARRAY_SIZE(address_pin_list),
                  GPIO_MODE_OUTPUT_50MHZ, GPIO_CNF_OUTPUT_GP_PUSH_PULL);
    /* Configure data lines */
    pin_list_conf(data_pin_list, ARRAY_SIZE(data_pin_list),
                  GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOATING);
    /* Configure USART1 TX */
    gpio_pin_conf(GPIO_A, 9,
                  GPIO_MODE_OUTPUT_50MHZ, GPIO_CNF_OUTPUT_AF_PUSH_PULL);
    /* Configure USART1 RX */
    gpio_pin_conf(GPIO_A, 10,
                  GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOATING);

    /*
     * Set chip-enable
     */
    pin_list_set(&ce_pin, 1, false);

    /*
     * Init the counter
     */
    COUNTER = 0;

    /*
     * Configure USART
     */
    /* Use USART1 */
    USART = USART1;
    /* Enable USART, leave the default mode of 8N1 */
    USART->cr1 |= USART_CR1_UE_MASK;
    /* Set baud rate of 115200 based on PCLK1 at 36MHz */
    USART->brr = usart_brr_val(72 * 1000 * 1000, 115200);
    /* Enable receiver and transmitter */
    USART->cr1 |= USART_CR1_RE_MASK | USART_CR1_TE_MASK;

    /*
     * Wait for Enter to be pressed
     */
    do {
        /* Wait for receive register to fill */
        while (!(USART->sr & USART_SR_RXNE_MASK));
        /* Read the byte */
        c = USART->dr;
    } while (c != '\r');

    /*
     * Set SysTick timer to fire each 100us for AHB clock of 72MHz.
     * The speed is chosen to let USART transmit one character each pulse.
     */
    STK->val = STK->load = 7200 - 1;
    /* Enable timer and interrupt, set clocksource to AHB (72MHz) */
    STK->ctrl |= STK_CTRL_ENABLE_MASK | STK_CTRL_TICKINT_MASK |
                 (STK_CTRL_CLKSOURCE_VAL_AHB << STK_CTRL_CLKSOURCE_LSB);

    STOP;
}
