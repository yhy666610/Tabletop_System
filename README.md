# Tabletop System — 智能桌搭系统

基于 **STM32F407** 和 **FreeRTOS** 的桌面信息显示系统，集成室内环境监测、网络时间同步与实时天气查询功能，通过 240×320 彩色 LCD 直观展示。

---

## 功能特性

| 功能 | 说明 |
|------|------|
| 🕐 时间 / 日期显示 | 通过 SNTP 自动校时，每小时同步一次，冒号每秒闪烁 |
| 🌡️ 室内温湿度 | 使用 AHT20 传感器，每 3 秒采集一次 |
| ⛅ 室外天气 | 调用心知天气 API，每分钟刷新一次 |
| 📶 WiFi 状态 | 实时显示已连接 SSID，断线自动提示 |
| 🖥️ 多页面 UI | 包含欢迎页、WiFi 连接页、主信息页及错误页 |

---

## 硬件组成

| 模块 | 型号 / 说明 |
|------|-------------|
| 主控 MCU | STM32F407（Cortex-M4，168 MHz） |
| 显示屏 | ST7789，240×320，SPI 接口 |
| 温湿度传感器 | AHT20，I²C 接口 |
| WiFi 模块 | ESP 系列（AT 指令模式，USART 接口） |
| 实时时钟 | STM32 片内 RTC，外部低速晶振（LSE）供时 |

---

## 软件架构

```
Tabletop_System/
├── app/               # 应用层
│   ├── main.c         # 入口，FreeRTOS 任务创建
│   ├── app.c/.h       # 应用主循环，定时器管理
│   ├── board.c        # 板级初始化
│   ├── wifi.c/.h      # WiFi 连接管理（SSID / 密码配置）
│   ├── weather.c/.h   # 心知天气 JSON 解析
│   ├── workqueue.c/.h # 工作队列（异步任务调度）
│   ├── ui.c/.h        # UI 绘制基础接口
│   ├── page/          # 页面：欢迎页、WiFi 页、主页、错误页
│   ├── component/     # 公共 UI 组件、日志 Shell 初始化
│   ├── font/          # 字体资源（Maple 系列多规格）
│   └── image/         # 图标资源（天气图标等）
├── driver/            # 硬件驱动层
│   ├── st7789.c/.h    # LCD 驱动（SPI + DMA）
│   ├── aht20.c/.h     # 温湿度传感器驱动（I²C）
│   ├── esp_at.c/.h    # ESP-AT 指令集封装（WiFi / SNTP / HTTP）
│   ├── rtc.c/.h       # RTC 驱动
│   ├── usart.c/.h     # USART 驱动
│   ├── led.c/.h       # LED 驱动
│   └── tim_delay.c/.h # 定时器延时
├── firmware/          # 固件层（CMSIS、标准外设库）
├── third_lib/         # 第三方库
│   ├── freertos/      # FreeRTOS 实时操作系统
│   ├── easylogger/    # EasyLogger 日志框架
│   └── lettershell/   # LetterShell 交互式 Shell
├── mdk/               # Keil MDK 工程文件
└── scripts/           # 辅助脚本（如图片头文件生成工具）
```

---

## 快速上手

### 环境要求

- **IDE**：Keil MDK 5（ARM Compiler 5 或 6）
- **调试器**：ST-Link / J-Link
- **硬件**：按上述硬件组成准备各模块并完成接线

### 编译与烧录

1. 使用 Keil MDK 打开 `mdk/stm32f407.uvprojx`。
2. 修改 `app/wifi.h` 中的 WiFi SSID 和密码：
   ```c
   #define WIFI_SSID     "your_ssid"
   #define WIFI_PASSWORD "your_password"
   ```
3. 修改 `app/app.c` 中的 `WEATHER_URL` 宏，替换为您自己的心知天气 API Key 和城市 ID（可在 [心知天气控制台](https://www.seniverse.com/) 免费申请）。
4. 点击 **Build**（F7）编译，无报错后通过 **Download**（F8）烧录到开发板。
5. 复位开发板，依次显示欢迎页 → WiFi 连接页 → 主信息页。

### 日志查看

系统通过 USART1（波特率 **115200**，8N1）输出日志，可使用串口调试工具查看运行状态。  
集成 LetterShell，支持通过串口终端执行交互命令。

---

## 第三方库

| 库 | 说明 |
|----|------|
| [FreeRTOS](https://www.freertos.org/) | 实时操作系统，提供任务调度、软件定时器等 |
| [EasyLogger](https://github.com/armink/EasyLogger) | 轻量级日志框架，支持多级别、多输出 |
| [LetterShell](https://github.com/NevermindZZT/letter-shell) | 嵌入式交互式 Shell |

---

## 版本

当前版本：**V1.0**

