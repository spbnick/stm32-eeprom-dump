#include <init.h>
#include <rcc.h>
#include <gpio.h>
#include <stk.h>
#include <misc.h>
#include <stddef.h>

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
const struct pin control_pin_list[3] = {
    [CONTROL_PIN_IDX_OE]    = PIN(B, 0),
    [CONTROL_PIN_IDX_CE]    = PIN(B, 1),
    [CONTROL_PIN_IDX_WE]    = PIN(B, 9),
};
const struct pin * const oe_pin = control_pin_list + CONTROL_PIN_IDX_OE;
const struct pin * const ce_pin = control_pin_list + CONTROL_PIN_IDX_CE;
const struct pin * const we_pin = control_pin_list + CONTROL_PIN_IDX_WE;

/* Address pins (LSB to MSB) */
const struct pin address_pin_list[19] = {
    [0]     = PIN(A, 12),
    [1]     = PIN(A, 15),
    [2]     = PIN(B, 3),
    [3]     = PIN(B, 4),
    [4]     = PIN(B, 5),
    [5]     = PIN(B, 6),
    [6]     = PIN(B, 7),
    [7]     = PIN(B, 8),
    [8]     = PIN(A, 4),
    [9]     = PIN(A, 5),
    [10]    = PIN(A, 7),
    [11]    = PIN(A, 6),
    [12]    = PIN(A, 0),
    [13]    = PIN(A, 3),
    [14]    = PIN(A, 2),
    [15]    = PIN(A, 1),
    [16]    = PIN(C, 13),
    [17]    = PIN(C, 14),
    [18]    = PIN(C, 15),
};

/* Data pins (LSB to MSB) */
const struct pin data_pin_list[8] = {
    [0]     = PIN(A, 11),
    [1]     = PIN(B, 11),
    [2]     = PIN(B, 10),
    [3]     = PIN(B, 12),
    [4]     = PIN(B, 13),
    [5]     = PIN(B, 14),
    [6]     = PIN(B, 15),
    [7]     = PIN(A, 8),
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

volatile unsigned int ADDR;

void systick_handler(void) __attribute__ ((isr));
void
systick_handler(void)
{
    pin_list_set(address_pin_list, ARRAY_SIZE(address_pin_list), ADDR);
    ADDR++;
}

void
reset(void)
{
    /* System init */
    init();

    /*
     * Configure I/O ports
     */
    /* Enable APB2 clock to I/O ports */
    RCC->apb2enr |= RCC_APB2ENR_IOPAEN_MASK |
                    RCC_APB2ENR_IOPBEN_MASK |
                    RCC_APB2ENR_IOPCEN_MASK;

    /* Configure control lines */
    pin_list_conf(control_pin_list, ARRAY_SIZE(control_pin_list),
                  GPIO_MODE_OUTPUT_50MHZ, GPIO_CNF_OUTPUT_AF_OPEN_DRAIN);
    /* Configure address lines */
    pin_list_conf(address_pin_list, ARRAY_SIZE(control_pin_list),
                  GPIO_MODE_OUTPUT_2MHZ, GPIO_CNF_OUTPUT_GP_PUSH_PULL);
    /* Configure data lines */
    pin_list_conf(data_pin_list, ARRAY_SIZE(control_pin_list),
                  GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOATING);

    /*
     * Start with address zero
     */
    ADDR = 0;

    /*
     * Set SysTick timer to fire the interrupt each half-second.
     * NOTE the ST PM0056 says: "When HCLK is programmed at the maximum
     * frequency, the SysTick period is 1ms."
     */
    STK->val = STK->load =
        (((STK->calib & STK_CALIB_TENMS_MASK) >> STK_CALIB_TENMS_LSB) + 1) *
        500 - 1;
    STK->ctrl |= STK_CTRL_ENABLE_MASK | STK_CTRL_TICKINT_MASK;

    STOP;
}
