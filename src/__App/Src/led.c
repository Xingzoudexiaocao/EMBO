/*
 * CTU/PillScope project
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include "cfg.h"
#include "led.h"
#include "main.h"


void led_init(led_data_t* self)
{
    self->ms = 0;
    self->num = 0;
    self->enabled = 0;
    self->uwtick_first = 0;
}

void led_set(led_data_t* self, uint8_t enable)
{
    self->enabled = enable;
    if (!self->enabled)
        PS_LED_PORT->BSRR |= (1 << PS_LED_PIN);  // 1
    else
        PS_LED_PORT->BRR |= (1 << PS_LED_PIN);   // 0
}

void led_toggle(led_data_t* self)
{
    if (self->enabled)
        PS_LED_PORT->BSRR |= (1 << PS_LED_PIN);  // 1
    else
        PS_LED_PORT->BRR |= (1 << PS_LED_PIN);   // 0
    self->enabled = !self->enabled;
}

void led_blink_set(led_data_t* self, int num, int ms)
{
    self->num = (num * 2) - 1;
    self->ms = ms;
    self->uwtick_first = uwTick;
    led_set(self, 1);
}

void led_blink_do(led_data_t* self)
{
    if (self->num > 0)
    {
        int diff;
        if (uwTick >= self->uwtick_first)
            diff = uwTick - self->uwtick_first;
        else
            diff = (uwTick - self->uwtick_first) + 4294967295;

        if (diff >= self->ms)
        {
            self->uwtick_first = uwTick;
            self->num--;
            led_toggle(self);
        }
    }
}
