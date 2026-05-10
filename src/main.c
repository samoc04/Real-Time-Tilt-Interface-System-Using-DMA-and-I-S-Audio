#include "stm32l4xx.h"
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/unistd.h>

#include "eeng1030_lib.h"
#include "i2c.h"
#include "audio_data.h"

#define NUM_LEDS   16
#define WS_PIN     7

#define BTN_PIN    3       // button moved to PB3

#define TOP_LED    0
#define DEAD_ZONE  180
#define SAVE_THRESHOLD 200

#define AUDIO_LIMIT 750
#define ADC_BUF_SIZE 32

uint16_t response;

int32_t X_g, Y_g, Z_g;
int32_t X_filt = 0, Y_filt = 0, Z_filt = 0;

int saved_mode = 0;
int saved_centre_led = 0;
int save_latched = 0;

volatile uint16_t adc_buffer[ADC_BUF_SIZE];
volatile uint16_t adc_average = 0;
volatile uint8_t timer_flag = 0;

const int16_t tone_silent[AUDIO_LENGTH] = {0};
const int16_t *current_tone = 0;

static const int16_t dir_x[NUM_LEDS] = {
     0,  383,  707,  924, 1000,  924,  707,  383,
     0, -383, -707, -924,-1000, -924, -707, -383
};

static const int16_t dir_y[NUM_LEDS] = {
   1000,  924,  707,  383,    0, -383, -707, -924,
  -1000, -924, -707, -383,    0,  383,  707,  924
};

void setup(void);
void delay(volatile uint32_t dly);
void initSerial(uint32_t baudrate);
void eputc(char c);

static void clock_init_80mhz(void);
static void gpio_init_ws2812(void);
static void gpio_init_button(void);
static int button_pressed(void);
static void dwt_init(void);
static void delay_us(uint32_t us);

static void initADC_DMA(void);
static void initTimer2PWM(void);
static void setTimer2Duty(int duty);
static void initTIM6Interrupt(void);

static void initSAI(void);
static void initAudioDMA(const int16_t *tone);
static void set_audio_tone(const int16_t *tone);
static void update_audio_warning(void);

static void ws2812_reset(void);
static void ws2812_send_bit(uint8_t bit);
static void ws2812_send_byte(uint8_t byte);
static void ws2812_send_pixel(uint8_t g, uint8_t r, uint8_t b);

static int32_t read_bmi160_axis(uint8_t reg_low);
static int wrap_led(int n);
static int get_pointer_led(int32_t x, int32_t y, int32_t z);
static void show_live_and_saved(int live_centre_led, int saved_centre_led, int saved_active,
                                uint8_t g, uint8_t r, uint8_t b);

int main(void)
{
    uint8_t colour_index = 0;
    uint8_t last_button_state = 0;

    uint8_t g_val = 0x00;
    uint8_t r_val = 0x08;
    uint8_t b_val = 0x00;

    clock_init_80mhz();
    setup();
    initSerial(9600);

    gpio_init_ws2812();
    gpio_init_button();
    dwt_init();

    // PA3 = TIM2_CH4 PWM output
    pinMode(GPIOA, 3, 2);
    GPIOA->AFR[0] &= ~(0xF << (4 * 3));
    GPIOA->AFR[0] |=  (1U << (4 * 3));

    initADC_DMA();
    initTimer2PWM();

    initSAI();
    initAudioDMA(tone_silent);

    initTIM6Interrupt();

    ResetI2C();

    I2CStart(0x69, WRITE, 2);
    I2CWrite(0x7E);
    I2CWrite(0x11);
    I2CStop();

    delay(1000000);

    // start SAI after DMA is ready
    SAI1_Block_A->CR1 |= (1 << 16);

    printf("Project 2 final integration started\r\n");

    while (1)
    {
        if (timer_flag)
        {
            timer_flag = 0;
            printf("ADC average = %d\r\n", adc_average);
        }

        uint8_t current_button_state = button_pressed();

        if ((current_button_state == 1) && (last_button_state == 0))
        {
            delay(30000);

            if (button_pressed() == 1)
            {
                colour_index++;

                if (colour_index >= 4)
                    colour_index = 0;

                while (button_pressed() == 1) {}

                delay(30000);
            }
        }

        last_button_state = current_button_state;

        X_g = read_bmi160_axis(0x12);
        Y_g = read_bmi160_axis(0x14);
        Z_g = read_bmi160_axis(0x16);

        X_filt = (3 * X_filt + X_g) / 4;
        Y_filt = (3 * Y_filt + Y_g) / 4;
        Z_filt = (3 * Z_filt + Z_g) / 4;

        int pot_value = adc_average;
        int centre_led = get_pointer_led(X_filt, Y_filt, Z_filt);

        if (pot_value <= SAVE_THRESHOLD)
        {
            setTimer2Duty(0);

            if (save_latched == 0)
            {
                saved_centre_led = centre_led;
                saved_mode = 1;
                save_latched = 1;
            }
        }
        else
        {
            setTimer2Duty(pot_value);
            saved_mode = 0;
            save_latched = 0;
        }

        if (colour_index == 0)
        {
            g_val = 0x00;
            r_val = 0x08;
            b_val = 0x00;
        }
        else if (colour_index == 1)
        {
            g_val = 0x08;
            r_val = 0x08;
            b_val = 0x00;
        }
        else if (colour_index == 2)
        {
            g_val = 0x02;
            r_val = 0x08;
            b_val = 0x00;
        }
        else
        {
            g_val = 0x08;
            r_val = 0x00;
            b_val = 0x00;
        }

        update_audio_warning();

        printf("POT=%d X=%ld Y=%ld Z=%ld colour=%d saved=%d saved_led=%d live_led=%d\r\n",
               pot_value, X_filt, Y_filt, Z_filt,
               colour_index, saved_mode, saved_centre_led, centre_led);

        show_live_and_saved(centre_led, saved_centre_led, saved_mode, g_val, r_val, b_val);

        delay(80000);
    }
}

static void update_audio_warning(void)
{
    if (X_filt > AUDIO_LIMIT ||
        X_filt < -AUDIO_LIMIT ||
        Y_filt > AUDIO_LIMIT ||
        Y_filt < -AUDIO_LIMIT)
    {
        set_audio_tone(audio_data);
    }
    else
    {
        set_audio_tone(tone_silent);
    }
}

static void set_audio_tone(const int16_t *tone)
{
    if (current_tone == tone)
        return;

    current_tone = tone;

    DMA2_Channel6->CCR &= ~(1 << 0);
    SAI1_Block_A->CR1 &= ~(1 << 16);

    DMA2_Channel6->CMAR = (uint32_t)tone;
    DMA2_Channel6->CNDTR = AUDIO_LENGTH;

    DMA2_Channel6->CCR |= (1 << 0);
    SAI1_Block_A->CR1 |= (1 << 16);
}

static void initSAI(void)
{
    // PA8  = BCLK
    // PA9  = LRC
    // PA10 = DIN

    pinMode(GPIOA, 8, 2);
    pinMode(GPIOA, 9, 2);
    pinMode(GPIOA, 10, 2);

    selectAlternateFunction(GPIOA, 8, 13);
    selectAlternateFunction(GPIOA, 9, 13);
    selectAlternateFunction(GPIOA, 10, 13);

    RCC->APB2ENR |= (1 << 21);

    RCC->CR &= ~(1 << 26);
    while (RCC->CR & (1 << 27)) {}

    RCC->PLLSAI1CFGR =
        (2 << 27) |
        (37 << 8) |
        (1 << 16);

    RCC->CR |= (1 << 26);
    while ((RCC->CR & (1 << 27)) == 0) {}

    SAI1->GCR = (1 << 4);

    SAI1_Block_A->CR1 = 0;
    SAI1_Block_A->CR2 = 0;
    SAI1_Block_A->SLOTR = 0;
    SAI1_Block_A->FRCR = 0;

    SAI1_Block_A->CR1 =
        (7 << 5)  |
        (3 << 20) |
        (1 << 9);

    SAI1_Block_A->SLOTR =
        (1 << 8)  |
        (1 << 16) |
        (1 << 17) |
        (1 << 7);

    SAI1_Block_A->FRCR =
        (1 << 18) |
        (1 << 16) |
        (15 << 8) |
        (32 - 1);

    SAI1_Block_A->CR1 |= (1 << 17);
}

static void initAudioDMA(const int16_t *tone)
{
    current_tone = tone;

    RCC->AHB1ENR |= (1 << 1);

    DMA2_Channel6->CCR &= ~(1 << 0);

    DMA2_Channel6->CMAR = (uint32_t)tone;
    DMA2_Channel6->CPAR = (uint32_t)&SAI1_Block_A->DR;
    DMA2_Channel6->CNDTR = AUDIO_LENGTH;

    DMA2_CSELR->CSELR &= ~(0xF << 20);
    DMA2_CSELR->CSELR |=  (5 << 20);

    DMA2_Channel6->CCR =
        (1 << 11) |
        (1 << 9)  |
        (1 << 8)  |
        (1 << 7)  |
        (1 << 5)  |
        (1 << 0);
}

static void initADC_DMA(void)
{
    // PA0 = ADC1_IN5 on STM32L432KC
    pinMode(GPIOA, 0, 3);

    RCC->AHB2ENR |= (1 << 13);
    RCC->AHB1ENR |= (1 << 0);

    if (ADC1->CR & ADC_CR_ADEN)
    {
        ADC1->CR |= ADC_CR_ADDIS;
        while (ADC1->CR & ADC_CR_ADEN) {}
    }

    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN;
    delay(10000);

    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL) {}

    DMA1_Channel1->CCR &= ~(1 << 0);

    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)adc_buffer;
    DMA1_Channel1->CNDTR = ADC_BUF_SIZE;

    DMA1_CSELR->CSELR &= ~(0xF << 0);

    DMA1_Channel1->CCR =
        (1 << 10) |
        (1 << 8)  |
        (1 << 7)  |
        (1 << 5);

    DMA1_Channel1->CCR |= (1 << 0);

    ADC1->CFGR = 0;
    ADC1->CFGR |= ADC_CFGR_CONT;
    ADC1->CFGR |= ADC_CFGR_DMAEN;
    ADC1->CFGR |= ADC_CFGR_DMACFG;

    ADC1->SMPR1 &= ~(7 << (5 * 3));
    ADC1->SMPR1 |=  (7 << (5 * 3));

    ADC1->SQR1 = 0;
    ADC1->SQR1 |= (5 << 6);

    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) {}

    ADC1->CR |= ADC_CR_ADSTART;
}

static void initTIM6Interrupt(void)
{
    RCC->APB1ENR1 |= (1 << 4);

    TIM6->PSC = 8000 - 1;
    TIM6->ARR = 1000 - 1;

    TIM6->DIER |= (1 << 0);

    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    TIM6->CR1 |= (1 << 0);
}

void TIM6_DAC_IRQHandler(void)
{
    if (TIM6->SR & (1 << 0))
    {
        TIM6->SR &= ~(1 << 0);

        uint32_t sum = 0;

        for (int i = 0; i < ADC_BUF_SIZE; i++)
        {
            sum += adc_buffer[i];
        }

        adc_average = sum / ADC_BUF_SIZE;
        timer_flag = 1;
    }
}

static int32_t read_bmi160_axis(uint8_t reg_low)
{
    int16_t accel_raw;
    int32_t value;

    I2CStart(0x69, WRITE, 1);
    I2CWrite(reg_low);
    I2CReStart(0x69, READ, 2);

    response = I2CRead();
    accel_raw = response;

    response = I2CRead();
    accel_raw = accel_raw + (response << 8);

    I2CStop();

    value = accel_raw;
    value = (value * 981) / 16384;

    return value;
}

static int wrap_led(int n)
{
    while (n < 0) n += NUM_LEDS;
    while (n >= NUM_LEDS) n -= NUM_LEDS;

    return n;
}

static int get_pointer_led(int32_t x, int32_t y, int32_t z)
{
    int32_t mag2_xy = x * x + y * y;

    if (mag2_xy < (DEAD_ZONE * DEAD_ZONE))
    {
        return TOP_LED;
    }

    int best_led = 0;
    int32_t best_dot = -2147483647;

    for (int i = 0; i < NUM_LEDS; i++)
    {
        int32_t dot = x * dir_x[i] + y * dir_y[i];

        if (dot > best_dot)
        {
            best_dot = dot;
            best_led = i;
        }
    }

    return wrap_led(TOP_LED + best_led);
}

static void show_live_and_saved(int live_centre_led, int saved_centre_led, int saved_active,
                                uint8_t g, uint8_t r, uint8_t b)
{
    int live_a = wrap_led(live_centre_led - 1);
    int live_b = wrap_led(live_centre_led);
    int live_c = wrap_led(live_centre_led + 1);

    int save_a = wrap_led(saved_centre_led - 1);
    int save_b = wrap_led(saved_centre_led);
    int save_c = wrap_led(saved_centre_led + 1);

    ws2812_reset();

    for (int i = 0; i < NUM_LEDS; i++)
    {
        uint8_t gg = 0x00;
        uint8_t rr = 0x00;
        uint8_t bb = 0x00;

        if (saved_active && (i == save_a || i == save_b || i == save_c))
        {
            gg = 0x00;
            rr = 0x00;
            bb = 0x08;
        }

        if (i == live_a || i == live_b || i == live_c)
        {
            gg = g;
            rr = r;
            bb = b;
        }

        ws2812_send_pixel(gg, rr, bb);
    }

    ws2812_reset();
}

void setup(void)
{
    RCC->AHB2ENR |= (1 << 0) | (1 << 1);
    initI2C();
}

void delay(volatile uint32_t dly)
{
    while (dly--) {}
}

void initSerial(uint32_t baudrate)
{
    RCC->AHB2ENR |= (1 << 0);

    pinMode(GPIOA, 2, 2);
    selectAlternateFunction(GPIOA, 2, 7);

    pinMode(GPIOA, 15, 2);
    selectAlternateFunction(GPIOA, 15, 3);

    RCC->APB1ENR1 |= (1 << 17);

    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = (1 << 12);
    USART2->BRR = 80000000 / baudrate;

    USART2->CR1 = (1 << 3);
    USART2->CR1 |= (1 << 2);
    USART2->CR1 |= (1 << 0);
}

int _write(int file, char *data, int len)
{
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO))
    {
        errno = EBADF;
        return -1;
    }

    int original_len = len;

    while (len--)
    {
        eputc(*data);
        data++;
    }

    return original_len;
}

void eputc(char c)
{
    while ((USART2->ISR & (1 << 6)) == 0) {}
    USART2->TDR = c;
}

static void clock_init_80mhz(void)
{
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}

    FLASH->ACR |= FLASH_ACR_LATENCY_4WS;

    RCC->PLLCFGR =
        RCC_PLLCFGR_PLLSRC_HSI |
        (0 << RCC_PLLCFGR_PLLM_Pos) |
        (10 << RCC_PLLCFGR_PLLN_Pos) |
        (0 << RCC_PLLCFGR_PLLR_Pos) |
        RCC_PLLCFGR_PLLREN;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    SystemCoreClock = 80000000;
}

static void gpio_init_ws2812(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    GPIOA->MODER &= ~(3U << (WS_PIN * 2));
    GPIOA->MODER |=  (1U << (WS_PIN * 2));

    GPIOA->OTYPER &= ~(1U << WS_PIN);

    GPIOA->OSPEEDR &= ~(3U << (WS_PIN * 2));
    GPIOA->OSPEEDR |=  (3U << (WS_PIN * 2));

    GPIOA->PUPDR &= ~(3U << (WS_PIN * 2));

    GPIOA->BSRR = (1U << (WS_PIN + 16));
}

static void gpio_init_button(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    GPIOB->MODER &= ~(3U << (BTN_PIN * 2));

    GPIOB->PUPDR &= ~(3U << (BTN_PIN * 2));
    GPIOB->PUPDR |=  (1U << (BTN_PIN * 2));
}

static int button_pressed(void)
{
    if ((GPIOB->IDR & (1U << BTN_PIN)) == 0)
        return 1;
    else
        return 0;
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks) {}
}

static void ws2812_reset(void)
{
    GPIOA->BSRR = (1U << (WS_PIN + 16));
    delay_us(80);
}

static void ws2812_send_bit(uint8_t bit)
{
    uint32_t start = DWT->CYCCNT;

    if (bit)
    {
        GPIOA->BSRR = (1U << WS_PIN);
        while ((DWT->CYCCNT - start) < 64) {}

        GPIOA->BSRR = (1U << (WS_PIN + 16));
        while ((DWT->CYCCNT - start) < 100) {}
    }
    else
    {
        GPIOA->BSRR = (1U << WS_PIN);
        while ((DWT->CYCCNT - start) < 32) {}

        GPIOA->BSRR = (1U << (WS_PIN + 16));
        while ((DWT->CYCCNT - start) < 100) {}
    }
}

static void ws2812_send_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        ws2812_send_bit((byte >> i) & 0x01);
    }
}

static void ws2812_send_pixel(uint8_t g, uint8_t r, uint8_t b)
{
    ws2812_send_byte(g);
    ws2812_send_byte(r);
    ws2812_send_byte(b);
}

static void initTimer2PWM(void)
{
    RCC->APB1ENR1 |= (1 << 0);

    TIM2->CR1 = 0;
    TIM2->CCMR2 = (0b110 << 12) | (1 << 11);
    TIM2->CCER |= (1 << 12);

    TIM2->PSC = 79;
    TIM2->ARR = 1000 - 1;
    TIM2->CCR4 = 0;

    TIM2->EGR |= (1 << 0);
    TIM2->CR1 |= (1 << 7);
    TIM2->CR1 |= (1 << 0);
}

static void setTimer2Duty(int duty)
{
    int arrvalue;

    if (duty < 0)
        duty = 0;

    if (duty > 4095)
        duty = 4095;

    if (duty <= SAVE_THRESHOLD)
    {
        TIM2->CCR4 = 0;
        return;
    }

    arrvalue = (duty * (TIM2->ARR + 1)) / 4095;
    TIM2->CCR4 = arrvalue;
}