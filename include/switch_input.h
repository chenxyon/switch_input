/**
 * @file     switch_input.h
 * @brief    SwitchInput —— 74HC165 并行输入移位寄存器驱动组件
 *
 * ╔══════════════════════════════════════════════════════════════╗
 * ║        永久全局规则（所有组件强制约束，后续不得违反）       ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║ 1. 组件必须用 class 封装（禁止纯 C 函数 / namespace 作 API）║
 * ║ 2. 配置通过构造函数 Config 结构体注入（禁止硬编码）         ║
 * ║ 3. 组件零板级依赖（禁止 include board_config.h / pin）      ║
 * ║ 4. 状态封装在实例里（禁止全局单例 / static 全局变量）       ║
 * ║ 5. 禁止 Arduino.h 冲突宏（RESET→SOFT_RESET / DISPLAY→IO_DISPLAY / ERROR→BAD_CMD）║
 * ║ 6. 兼容旧 API 用 #define CMD_XXX 别名                      ║
 * ║ 7. 组件目录结构：include/ + src/（PlatformIO 自动识别）     ║
 * ║ 8. 板子侧只做组装：main.cpp 实例化 + 传 Config              ║
 * ║ 9. GPIO 扩展优先：同类 GPIO ≥ 2 个时，优先用类似串行扩展器       ║
 * ║    （如 74HC595/165、PCA9685、CD4021/4094、MCP23017/ADS1115 等）║
 * ║    芯片型号+数量+节约引脚数在注释中列出，由用户审核后实施        ║
 * ║ 10. 版本强制：所有自定义组件必须接入 version_manager，       ║
 * ║     包含 xxx_version.h + version.txt + COMPONENT_VERSION_   ║
 * ║     REGISTER() 宏调用，禁止跳过。板子固件也必须注册自身版本   ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * 📌 本组件即为规则 9 触发的典型案例（3 个重启按钮 → 1×74HC165，共享 595 的 SPI）
 *
 * 硬件：74HC165 × N 片级联
 *   主控引脚：QH(数据出) + CLK(时钟) + SH/LD(锁存)
 *   级联：U1.QH → U2.SER（主控从最后一片 U2.QH 读）
 *
 * 重要！165 没有内部上拉，外部必须加 10kΩ 上拉电阻
 * 按下 = LOW（上拉到 HIGH，按钮接地拉低）
 *
 * 软件消抖：组件内部做连续 N 次一致才确认 debounceCount 次
 *   debounceMs 间隔采样
 *
 * 使用示例：
 *   SwitchInput si({ .dataPin=41, .clockPin=39, .loadPin=42, .chainLen=1,
 *                    .debounceCount=3, .debounceIntervalMs=5 });
 *   si.init();
 *   if (si.isPressed(0)) { ... }
 */
#ifndef SWITCH_INPUT_H
#define SWITCH_INPUT_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

class SwitchInput {
public:
    struct Config {
        gpio_num_t dataPin;          /* 165 QH (串行数据出 → 主控读) */
        gpio_num_t clockPin;         /* 165 CLK (移位时钟) */
        gpio_num_t loadPin;          /* 165 SH/LD (并行锁存) */
        uint8_t    chainLen = 1;     /* 级联片数 */
        uint8_t    debounceCount = 3; /* 消抖：连续 N 次一致才确认 */
        uint8_t    debounceIntervalMs = 5; /* 每次采样间隔 ms */
        const char* tag = "SwitchInput";
    };

    explicit SwitchInput(const Config& cfg);
    ~SwitchInput() = default;

    esp_err_t init();

    /* 读全部输入原始状态（bit0=D0, bit1=D1, ...），LOW=按钮按下 */
    uint16_t readRaw();

    /* 读第 n 路（n=0~chainLen*8-1），返回 true=LOW=按下 */
    bool isPressed(uint8_t n);

    /* 消抖后的稳定状态（连续读 debounceCount 次确认）*/
    bool isPressedDebounced(uint8_t n);

private:
    Config    cfg_;
    uint16_t  stableState_ = 0xFFFF;   /* 默认全 HIGH（上拉）*/

    /* 内部：单次从 165 读所有位 */
    uint16_t shiftInAll();
};

#endif /* SWITCH_INPUT_H */
