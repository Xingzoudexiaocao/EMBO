/*
 * CTU/PillScope project
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cfg.h"
#include "daq.h"
#include "daq_trig.h"
#include "utility.h"
#include "periph.h"
#include "main.h"


static void daq_enable_adc(daq_data_t* self, ADC_TypeDef* adc, uint8_t enable, uint32_t dma_ch);
static void daq_malloc(daq_data_t* self, daq_buff_t* buff, int mem, int reserve, int chans, uint32_t src, uint32_t dma_ch,
                       DMA_TypeDef* dma, enum daq_bits bits);
static void daq_clear_buff(daq_buff_t* buff);


void daq_init(daq_data_t* self)
{
    daq_trig_init(self);
    daq_settings_init(self);
    daq_settings_save(&self->save_s, &self->trig.save_s, &self->set, &self->trig.set);
    self->mode = SCOPE;

    daq_clear_buff(&self->buff1);
    daq_clear_buff(&self->buff2);
    daq_clear_buff(&self->buff3);
    daq_clear_buff(&self->buff4);
    daq_clear_buff(&self->buff_out);
    memset(self->buff_raw, 0, PS_DAQ_MAX_MEM * sizeof(uint8_t));
    self->buff_raw_ptr = 0;

    self->trig.buff_trig = NULL;
    self->buff_out.reserve = 0;
    self->enabled = 0;
    self->dis_hold = 0;
    self->vcc = 0;
    self->vcc_mv = 0;
    self->adc_max_val = 0;

    NVIC_SetPriority(PS_IRQN_DAQ_TIM, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), PS_IT_PRI_TIM3, 0));
    NVIC_SetPriority(PS_IRQN_ADC, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), PS_IT_PRI_ADC, 0));

    NVIC_EnableIRQ(PS_IRQN_DAQ_TIM);
    NVIC_EnableIRQ(PS_IRQN_ADC);

    NVIC_DisableIRQ(PS_LA_IRQ_EXTI1);
    NVIC_DisableIRQ(PS_LA_IRQ_EXTI2);
    NVIC_DisableIRQ(PS_LA_IRQ_EXTI3);
    NVIC_DisableIRQ(PS_LA_IRQ_EXTI4);

    adc_init();
}

void daq_settings_save(daq_settings_t* src1, trig_settings_t* src2, daq_settings_t* dst1, trig_settings_t* dst2)
{
    dst1->fs = src1->fs;
    dst1->mem = src1->mem;
    dst1->bits = src1->bits;

    dst1->ch1_en = src1->ch1_en;
    dst1->ch2_en = src1->ch2_en;
    dst1->ch3_en = src1->ch3_en;
    dst1->ch4_en = src1->ch4_en;

    dst2->val_percent = src2->val_percent;
    dst2->val = src2->val;
    dst2->ch = src2->ch;
    dst2->edge = src2->edge;
    dst2->mode = src2->mode;
    dst2->pretrigger = src2->pretrigger;
}

void daq_settings_init(daq_data_t* self)
{
    // SCOPE
    self->save_s.fs = 1000;
    self->save_s.mem = 500;
    self->save_s.bits = B12;

    self->save_s.ch1_en = 1;
    self->save_s.ch2_en = 1;
    self->save_s.ch3_en = 0;
    self->save_s.ch4_en = 0;

    self->trig.save_s.val_percent = 50;
    self->trig.save_s.val = 2047;
    self->trig.save_s.ch = 1;
    self->trig.save_s.edge = RISING;
    self->trig.save_s.mode = DISABLED;
    self->trig.save_s.pretrigger = 50;

    // LA
    self->save_l.fs = 100000;
    self->save_l.mem = 1000;
    self->save_l.bits = B1;

    self->save_l.ch1_en = 1;
    self->save_l.ch2_en = 1;
    self->save_l.ch3_en = 1;
    self->save_l.ch4_en = 1;

    self->trig.save_s.val_percent = 0;
    self->trig.save_l.val = 0;
    self->trig.save_l.ch = 1;
    self->trig.save_l.edge = RISING;
    self->trig.save_l.mode = AUTO;
    self->trig.save_l.pretrigger = 50;
}

int daq_mem_set(daq_data_t* self, uint16_t mem_per_ch)
{
    daq_enable(self, 0);
    daq_reset(self);

    self->buff_out.reserve = 0;
    daq_clear_buff(&self->buff1);
    daq_clear_buff(&self->buff2);
    daq_clear_buff(&self->buff3);
    daq_clear_buff(&self->buff4);
    daq_clear_buff(&self->buff_out);
    self->buff_raw_ptr = 0;
    memset(self->buff_raw, 0, PS_DAQ_MAX_MEM * sizeof(uint8_t));

    int max_len = PS_DAQ_MAX_MEM;
    int out_per_ch = mem_per_ch;
    if (self->set.bits == B12)
    {
        max_len /= 2;
        out_per_ch *= 2;
    }

    if (self->mode != LA)
    {
#if defined(PS_ADC_MODE_ADC1)

        int len1 = self->set.ch1_en + self->set.ch2_en + self->set.ch3_en + self->set.ch4_en + 1;

        //                            DMA ADC            UART / USB OUT
        if (mem_per_ch < 0 || (mem_per_ch * len1) + (mem_per_ch * (len1 - 1)) > max_len)
            return -2;

        daq_malloc(self, &self->buff1, mem_per_ch * len1, PS_MEM_RESERVE, len1, PS_ADC_ADDR(ADC1), PS_DMA_CH_ADC1, PS_DMA_ADC, self->set.bits);

        self->buff_out.chans = len1 - 1;
        self->buff_out.len = out_per_ch * (len1 - 1);

#elif defined(PS_ADC_MODE_ADC12)

        int len1 = self->set.ch1_en + self->set.ch2_en + 1;
        int len2 = self->set.ch3_en + self->set.ch4_en;
        int total = len1 + len2;

        if (mem_per_ch < 0 || (mem_per_ch * total) + (mem_per_ch * (total - 1)) > max_len)
            return -2;

        daq_malloc(self, &self->buff1, mem_per_ch * len1, PS_ADC_MEM_RESERVE, len1, PS_ADC_ADDR(ADC1), PS_DMA_CH_ADC1, PS_DMA_ADC, self->set.bits);
        daq_malloc(self, &self->buff2, mem_per_ch * len2, PS_ADC_MEM_RESERVE, len2, PS_ADC_ADDR(ADC2), PS_DMA_CH_ADC2, PS_DMA_ADC, self->set.bits);

        self->buff_out.chans = total - 1;
        self->buff_out.len = out_per_ch * (total - 1);

#elif defined(PS_ADC_MODE_ADC1234)

        int len1 = self->set.ch1_en + 1;
        int len2 = self->set.ch3_en;
        int len3 = self->set.ch3_en;
        int len4 = self->set.ch3_en;
        int total = len1 + len2 + len3 + len4;

        if (mem_per_ch < 0 || (mem_per_ch * total) + (mem_per_ch * (total - 1)) > max_len)
            return -2;

        daq_malloc(self, &self->buff1, mem_per_ch * len1, PS_ADC_MEM_RESERVE, len1, PS_ADC_ADDR(ADC1), PS_DMA_CH_ADC1, PS_DMA_ADC, self->set.bits);
        daq_malloc(self, &self->buff2, mem_per_ch * len2, PS_ADC_MEM_RESERVE, len2, PS_ADC_ADDR(ADC2), PS_DMA_CH_ADC2, PS_DMA_ADC, self->set.bits);
        daq_malloc(self, &self->buff3, mem_per_ch * len3, PS_ADC_MEM_RESERVE, len3, PS_ADC_ADDR(ADC3), PS_DMA_CH_ADC3, PS_DMA_ADC, self->set.bits);
        daq_malloc(self, &self->buff4, mem_per_ch * len4, PS_ADC_MEM_RESERVE, len4, PS_ADC_ADDR(ADC4), PS_DMA_CH_ADC4, PS_DMA_ADC, self->set.bits);

        self->buff_out.chans = total - 1;
        self->buff_out.len = out_per_ch * (total - 1);

#endif

        self->buff_out.data = (uint8_t*)(((uint8_t*)self->buff_raw)+(self->buff_raw_ptr));
        self->buff_raw_ptr += self->buff_out.len;
    }
    else // mode == LA
    {
        if (mem_per_ch < 0 || mem_per_ch > PS_DAQ_MAX_MEM)
            return -2;

        daq_malloc(self, &self->buff1, mem_per_ch, PS_MEM_RESERVE, 4, PS_DAQ_PORT->ODR, PS_DMA_CH_LA, PS_DMA_LA, self->set.bits);

        self->buff_out.chans = 4;
        self->buff_out.len = mem_per_ch;
        self->buff_out.data = (uint8_t*)(((uint8_t*)self->buff_raw)+(self->buff_raw_ptr));
        self->buff_raw_ptr += mem_per_ch;
    }

    self->set.mem = mem_per_ch;

    daq_trig_update(self);
    daq_enable(self, 1);
    return 0;
}

static void daq_malloc(daq_data_t* self, daq_buff_t* buff, int mem, int reserve, int chans, uint32_t src,
                       uint32_t dma_ch, DMA_TypeDef* dma, enum daq_bits bits)
{
    mem += reserve * chans;
    buff->reserve = reserve * chans;

    if (bits == B12)
    {
        size_t ln = mem * sizeof(uint16_t);
        //buff->data = (uint16_t*) malloc(ln);
        buff->data = (uint16_t*)(((uint8_t*)self->buff_raw)+(self->buff_raw_ptr));
        self->buff_raw_ptr += mem * 2;

        buff->chans = chans;
        buff->len = mem;
        memset(buff->data, 0, ln);
        dma_set(src, dma, dma_ch, (uint32_t)((uint16_t*)((uint8_t*)buff->data)), mem, LL_DMA_PDATAALIGN_HALFWORD);
    }
    else if (bits == B8)
    {
        size_t ln = mem * sizeof(uint8_t);
        //buff->data = (uint8_t*) malloc(ln);
        buff->data = (uint8_t*)(((uint8_t*)self->buff_raw)+(self->buff_raw_ptr));
        self->buff_raw_ptr += mem;
        buff->chans = chans;
        buff->len = mem;
        memset(buff->data, 0, ln);
        dma_set(src, dma, dma_ch, (uint32_t)((uint8_t*)buff->data), mem, LL_DMA_PDATAALIGN_BYTE);
    }
    else // if (bits == B1)
    {
        size_t ln = mem * sizeof(uint8_t);
        //buff->data = (uint8_t*) malloc(ln);
        buff->data = (uint8_t*)(((uint8_t*)self->buff_raw)+(self->buff_raw_ptr));
        self->buff_raw_ptr += mem;
        buff->chans = chans;
        buff->len = mem;
        memset(buff->data, 0, ln);
        dma_set(src, dma, PS_DMA_CH_LA, (uint32_t)((uint8_t*)buff->data), mem, LL_DMA_PDATAALIGN_BYTE);  // TODO DMA2 ??
    }
}

static void daq_clear_buff(daq_buff_t* buff)
{
    buff->data = NULL;
    buff->chans = 0;
    buff->len = 0;
    buff->reserve = 0;
}

int daq_bit_set(daq_data_t* self, enum daq_bits bits)
{
    if (bits != B12 && bits != B8 && bits != B1)
        return -1;

    self->set.bits = bits;
    if (bits == B12)
        self->adc_max_val = 4095;
    else if (bits == B8)
        self->adc_max_val = 255;
    else
        self->adc_max_val = 1;

    if (self->mode == SCOPE || self->mode == VM)
    {
        daq_enable(self, 0);
        daq_reset(self);

        if (bits == B8)
        {
#ifndef PS_ADC_BIT8
            return -2;
#endif
        }

        uint32_t bits_raw = LL_ADC_RESOLUTION_12B;
#ifdef PS_ADC_BIT8
        if (bits == B8)
            bits_raw = LL_ADC_RESOLUTION_8B;
#endif

#if defined(PS_ADC_MODE_ADC1) || defined(PS_ADC_MODE_ADC12) || defined(PS_ADC_MODE_ADC1234)
        adc_set_res(ADC1, bits_raw);
#endif

#if defined(PS_ADC_MODE_ADC12) || defined(PS_ADC_MODE_ADC1234)
        adc_set_res(ADC2, bits_raw);
#endif

#if defined(PS_ADC_MODE_ADC1234)
        adc_set_res(ADC3, bits_raw);
        adc_set_res(ADC4, bits_raw);
#endif
        int ret = daq_mem_set(self, self->set.mem);

        daq_enable(self, 1);

        return ret;
    }
    return 0;
}

int daq_fs_set(daq_data_t* self, float fs)
{
#if defined(PS_ADC_MODE_ADC1)
    float scope_max_fs = 1.0 / (PS_ADC_1CH_SMPL_TM * (float)(self->set.ch1_en + self->set.ch2_en + self->set.ch3_en + self->set.ch4_en + 1));
#elif defined(PS_ADC_MODE_ADC12)
    int adc1 = self->set.ch1_en + self->set.ch2_en + 1;
    int adc2 = self->set.ch3_en + self->set.ch4_en;
    float scope_max_fs = 1.0 / (PS_ADC_1CH_SMPL_TM * (float)(adc1 > adc2 ? adc1 : adc2));
#elif defined(PS_ADC_MODE_ADC1234)
    float scope_max_fs = 1.0 / (PS_ADC_1CH_SMPL_TM * (float)(self->set.ch1_en ? 2 : 1));
#endif
    if (fs < 0 || fs > (self->mode == LA ? PS_LA_MAX_FS : scope_max_fs))
        return -1;

    self->set.fs = fs;

    daq_enable(self, 0);
    daq_reset(self);

    int prescaler = 1;
    int reload = 1;
    self->set.fs = get_freq(&prescaler, &reload, PS_TIM_ADC_MAX, PS_TIM_ADC_FREQ, fs);

    LL_TIM_SetPrescaler(PS_TIM_ADC, prescaler);
    LL_TIM_SetAutoReload(PS_TIM_ADC, reload);

    daq_trig_update(self);
    daq_enable(self, 1);
    return 0;
}

int daq_ch_set(daq_data_t* self, uint8_t ch1, uint8_t ch2, uint8_t ch3, uint8_t ch4)
{
    self->set.ch1_en = ch1;
    self->set.ch2_en = ch2;
    self->set.ch3_en = ch3;
    self->set.ch4_en = ch4;

    int reen = 0;
    if (self->enabled)
    {
        reen = 1;
        daq_enable(self, 0);
        daq_reset(self);
    }

    if (self->mode != LA)
    {
#if defined(PS_ADC_MODE_ADC1)
        adc_set_ch(ADC1, ch1, ch2, ch3, ch4, PS_ADC_SMPL_TIME, 1);
#elif defined(PS_ADC_MODE_ADC12)
        adc_set_ch(ADC1, ch1, ch2, 0, 0, PS_ADC_SMPL_TIME, 1);
        adc_set_ch(ADC2, 0, 0, ch3, ch4, PS_ADC_SMPL_TIME, 0); // TODO MOZNA........... vcc tam, kde je trigger, jinak 1 => parametrizovat PS_DMA_LAST_IDX(daq.buff1.len, PS_DMA_ADC1);
#elif defined(PS_ADC_MODE_ADC1234)
        adc_set_ch(ADC1, ch1, 0, 0, 0, PS_ADC_SMPL_TIME, 1);
        adc_set_ch(ADC2, 0, ch2, 0, 0, PS_ADC_SMPL_TIME, 0);
        adc_set_ch(ADC1, 0, 0, ch3, 0, PS_ADC_SMPL_TIME, 0);
        adc_set_ch(ADC2, 0, 0, 0, ch4, PS_ADC_SMPL_TIME, 0);
#endif
    }

    int ret = daq_mem_set(self, self->set.mem);

    if (reen)
        daq_enable(self, 1);
    return ret;
}

void daq_reset(daq_data_t* self)
{
    self->trig.uwtick_first = 0;
    self->trig.pretrig_cntr = 0;
    self->trig.posttrig_size = 0;
    self->trig.ready_last = 0;
    self->trig.ready = 0;
    self->trig.cntr = 0;
    self->trig.all_cntr = 0;
    //self->trig.pos_frst = 0;
    //self->trig.pos_trig = 0;
    //self->trig.pos_last = 0;
    //self->trig.pos_diff = 0;
    self->trig.pretrig_cntr = 0;
    self->trig.is_post = 0;

    if (self->buff1.len > 0)
        memset(self->buff1.data, 0, self->buff1.len);
    if (self->buff2.len > 0)
        memset(self->buff2.data, 0, self->buff2.len);
    if (self->buff3.len > 0)
        memset(self->buff3.data, 0, self->buff3.len);
    if (self->buff4.len > 0)
        memset(self->buff4.data, 0, self->buff4.len);
}

void daq_enable(daq_data_t* self, uint8_t enable)
{
    self->trig.pretrig_cntr = 0;

    if (!enable)
        LL_TIM_DisableCounter(PS_TIM_ADC);

    if (self->enabled && self->dis_hold)
        return;

    if (self->mode == SCOPE || self->mode == VM)
    {
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH1, LL_GPIO_MODE_ANALOG);
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH2, LL_GPIO_MODE_ANALOG);
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH3, LL_GPIO_MODE_ANALOG);
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH4, LL_GPIO_MODE_ANALOG);

#if defined(PS_ADC_MODE_ADC1) || defined(PS_ADC_MODE_ADC12) || defined(PS_ADC_MODE_ADC1234)
        daq_enable_adc(self, ADC1, enable, PS_DMA_CH_ADC1);
#endif

#if defined(PS_ADC_MODE_ADC12) || defined(PS_ADC_MODE_ADC1234)
        daq_enable_adc(self, ADC2, enable, PS_DMA_CH_ADC2);
#endif

#if defined(PS_ADC_MODE_ADC1234)
        daq_enable_adc(self, ADC3, enable, PS_DMA_CH_ADC3);
        daq_enable_adc(self, ADC4, enable, PS_DMA_CH_ADC4);
#endif
    }
    else //if(self->mode == LA)
    {
        ASSERT(self->trig.exti_trig != 0);

        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH1, LL_GPIO_MODE_FLOATING);
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH2, LL_GPIO_MODE_FLOATING);
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH3, LL_GPIO_MODE_FLOATING);
        LL_GPIO_SetPinMode(PS_DAQ_PORT, PS_DAQ_CH4, LL_GPIO_MODE_FLOATING);

        if (enable)
        {
            LL_DMA_EnableChannel(PS_DMA_LA, PS_DMA_CH_LA);
            LL_TIM_EnableDMAReq_CC1(PS_TIM_ADC);
            if (self->trig.set.mode != DISABLED)
                NVIC_EnableIRQ(self->trig.exti_trig);
        }
        else
        {
            //LL_DMA_DisableChannel(PS_DMA_LA, PS_DMA_CH_LA);
            LL_TIM_DisableDMAReq_CC1(PS_TIM_ADC);
            NVIC_DisableIRQ(self->trig.exti_trig);
        }
    }
    if (enable)
        LL_TIM_EnableCounter(PS_TIM_ADC);
    else
        for (int i = 0; i < 10000; i++) __asm("nop"); // let DMA and ADC finish their jobs

    self->enabled = enable;
    self->trig.uwtick_first = uwTick;
}

static void daq_enable_adc(daq_data_t* self, ADC_TypeDef* adc, uint8_t enable, uint32_t dma_ch)
{
    if (enable)
    {
        LL_DMA_EnableChannel(PS_DMA_ADC, dma_ch);
        LL_ADC_REG_StartConversionExtTrig(adc, LL_ADC_REG_TRIG_EXT_RISING);
        if (self->trig.set.mode != DISABLED)
        {
            ASSERT(self->trig.ch_reg != 0);
            LL_ADC_SetAnalogWDMonitChannels(adc, self->trig.ch_reg);
        }
    }
    else
    {
        LL_ADC_REG_StopConversionExtTrig(adc);
        //LL_DMA_DisableChannel(PS_DMA_ADC, dma_ch);  // DO NOT USE!
        LL_ADC_SetAnalogWDMonitChannels(adc, LL_ADC_AWD_DISABLE);
    }
}

void daq_mode_set(daq_data_t* self, enum daq_mode mode)
{
    if (self->mode == SCOPE)
        daq_settings_save(&self->set, &self->trig.set, &self->save_s, &self->trig.save_s);
    else if (self->mode == LA)
        daq_settings_save(&self->set, &self->trig.set, &self->save_l, &self->trig.save_l);

    daq_enable(self, 0);
    daq_reset(self);
    self->dis_hold = 1;
    self->mode = mode;

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = PS_DAQ_CH1 | PS_DAQ_CH2 | PS_DAQ_CH3 | PS_DAQ_CH4;

    if (mode == SCOPE) // save settings
    {
        GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
        LL_GPIO_Init(PS_DAQ_PORT, &GPIO_InitStruct);

        daq_mem_set(self, 3); // safety guard
        daq_bit_set(self, self->save_s.bits);
        daq_ch_set(self, self->save_s.ch1_en, self->save_s.ch2_en, self->save_s.ch3_en, self->save_s.ch4_en);
        daq_fs_set(self, self->save_s.fs);
        daq_mem_set(self, self->save_s.mem);
        daq_trig_set(self, self->trig.save_s.ch, self->trig.save_s.val, self->trig.save_s.edge,
                     self->trig.save_s.mode, self->trig.save_s.pretrigger);
    }
    else if (mode == VM)
    {
        GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
        LL_GPIO_Init(PS_DAQ_PORT, &GPIO_InitStruct);

        daq_mem_set(self, 3); // safety guard
        daq_bit_set(self, B12);
        daq_ch_set(self, 1, 1, 1, 1);
        daq_mem_set(self, 200);
        daq_fs_set(self, 100);
        daq_trig_set(self, 0, 0, RISING, DISABLED, 50);
    }
    else // if (mode == LA)
    {
        GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
        LL_GPIO_Init(PS_DAQ_PORT, &GPIO_InitStruct);

        daq_mem_set(self, 3); // safety guard
        daq_bit_set(self, self->save_l.bits);
        daq_ch_set(self, self->save_l.ch1_en, self->save_l.ch2_en, self->save_l.ch3_en, self->save_l.ch4_en);
        daq_fs_set(self, self->save_l.fs);
        daq_mem_set(self, self->save_l.mem);
        daq_trig_set(self, self->trig.save_l.ch, self->trig.save_l.val, self->trig.save_l.edge,
                     self->trig.save_l.mode, self->trig.save_l.pretrigger);
    }

    self->dis_hold = 0;
    daq_enable(self, 1);
}
