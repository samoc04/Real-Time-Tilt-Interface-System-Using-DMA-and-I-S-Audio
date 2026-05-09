#include "stm32l4xx.h"
#include <stdint.h>

#define DIN_PIN    7   // PA7
#define BCLK_PIN   5   // PA5
#define LRC_PIN    4   // PA4

static void delay_cycles(volatile uint32_t d)
{
    while (d--) {}
}

static void pin_high(uint32_t pin)
{
    GPIOA->BSRR = (1U << pin);
}

static void pin_low(uint32_t pin)
{
    GPIOA->BSRR = (1U << (pin + 16));
}

static void send_bit(uint8_t bit)
{
    if (bit)
        pin_high(DIN_PIN);
    else
        pin_low(DIN_PIN);

    pin_high(BCLK_PIN);
    delay_cycles(20);
    pin_low(BCLK_PIN);
    delay_cycles(20);
}

static void send_16(uint16_t value)
{
    for (int i = 15; i >= 0; i--)
    {
        send_bit((value >> i) & 1U);
    }
}

static void send_stereo_sample(int16_t sample)
{
    // left channel
    pin_low(LRC_PIN);
    send_16((uint16_t)sample);

    // right channel
    pin_high(LRC_PIN);
    send_16((uint16_t)sample);
}

int main(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    GPIOA->MODER &= ~(
        (3U << (DIN_PIN * 2)) |
        (3U << (BCLK_PIN * 2)) |
        (3U << (LRC_PIN * 2))
    );

    GPIOA->MODER |= (
        (1U << (DIN_PIN * 2)) |
        (1U << (BCLK_PIN * 2)) |
        (1U << (LRC_PIN * 2))
    );

    GPIOA->OTYPER &= ~(
        (1U << DIN_PIN) |
        (1U << BCLK_PIN) |
        (1U << LRC_PIN)
    );

    GPIOA->OSPEEDR |= (
        (3U << (DIN_PIN * 2)) |
        (3U << (BCLK_PIN * 2)) |
        (3U << (LRC_PIN * 2))
    );

    while (1)
    {
        for (int i = 0; i < 200; i++)
        {
            send_stereo_sample(30000);
        }

        for (int i = 0; i < 200; i++)
        {
            send_stereo_sample(-30000);
        }
    }
}