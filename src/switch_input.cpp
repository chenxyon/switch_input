/**
 * @file     switch_input.cpp
 * @brief    SwitchInput —— 74HC165 实现（纯 GPIO bit-bang + 软件消抖）
 *
 * 修改：2026-11-23 新建
 */
#include "switch_input.h"
#include <Arduino.h>
#include "esp_log.h"

static const char* TAG = "SwitchInput";

SwitchInput::SwitchInput(const Config& cfg) : cfg_(cfg) {}

esp_err_t SwitchInput::init()
{
    pinMode(cfg_.clockPin, OUTPUT);
    pinMode(cfg_.loadPin,  OUTPUT);
    pinMode(cfg_.dataPin,  INPUT);    /* 165 QH → 主控 */

    digitalWrite(cfg_.clockPin, LOW);
    digitalWrite(cfg_.loadPin,  HIGH);  /* 空闲：HIGH=允许移位，LOW=锁存 */

    stableState_ = 0xFFFF;  /* 全 HIGH（上拉默认）*/

    ESP_LOGI(TAG, "SwitchInput 就绪 (chain=%d, data=%d clk=%d load=%d, debounce=%d×%dms)",
             cfg_.chainLen, cfg_.dataPin, cfg_.clockPin, cfg_.loadPin,
             cfg_.debounceCount, cfg_.debounceIntervalMs);
    return ESP_OK;
}

uint16_t SwitchInput::readRaw()
{
    return shiftInAll();
}

bool SwitchInput::isPressed(uint8_t n)
{
    if (n >= cfg_.chainLen * 8) return false;
    uint16_t raw = shiftInAll();
    return (raw >> n) & 1 ? false : true;   /* LOW=按下 */
}

bool SwitchInput::isPressedDebounced(uint8_t n)
{
    if (n >= cfg_.chainLen * 8) return false;

    /* 连续读 N 次，全部一致才更新 stableState_ */
    bool allSame = true;
    uint16_t last = shiftInAll();
    for (uint8_t i = 1; i < cfg_.debounceCount; i++) {
        delay(cfg_.debounceIntervalMs);
        uint16_t cur = shiftInAll();
        if (cur != last) { allSame = false; break; }
        last = cur;
    }
    if (allSame) stableState_ = last;

    /* 返回稳定状态，LOW=按下 */
    return (stableState_ >> n) & 1 ? false : true;
}

/* ====================================================================
 *  核心：从 74HC165 串行读所有位
 *  74HC165 时序：
 *    SH/LD LOW  → 锁存并行 D0~D7 到内部寄存器
 *    SH/LD HIGH → 允许移位
 *    CLK 上升沿 → 下一个位从 QH 移出
 *    高位先出还是低位先出？74HC165 是 D0→QH 先出（低位先）
 * ==================================================================== */
uint16_t SwitchInput::shiftInAll()
{
    /* 先锁存：LOW → 并行输入被装入 */
    digitalWrite(cfg_.loadPin, LOW);
    digitalWrite(cfg_.loadPin, HIGH);

    uint16_t result = 0;
    uint8_t  totalBits = cfg_.chainLen * 8;

    /* 低位先出：先读 U1.D0, D1, ... D7, U2.D0, ... */
    for (uint8_t i = 0; i < totalBits; i++) {
        digitalWrite(cfg_.clockPin, HIGH);
        int bit = digitalRead(cfg_.dataPin);
        digitalWrite(cfg_.clockPin, LOW);
        if (bit) result |= (uint16_t)1 << i;
    }

    return result;
}

#include "switch_input_version.h"
#include "version_manager.h"
COMPONENT_VERSION_REGISTER(switch_input, SWITCH_INPUT_VERSION_STRING)
