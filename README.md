# AI Passport Baye (三国霸业)

基于经典文曲星 / 步步高（BBK）《三国霸业》的原生 C 游戏核心（iBaye），为 **FoloToy AI Passport**（ESP32-C3）硬件深度打造的原生平台移植。

作为一个完全独立的嵌入式应用，在 ESP32-C3 单核 RISC-V 架构与 ST7789 彩屏上直接运行，提供原汁原味的掌上经典三国策略体验。

---

## 硬件规格与运行基线

| 项目 | 规格 / 测量基准 | 说明 |
| :--- | :--- | :--- |
| **主控芯片** | ESP32-C3 (QFN32, rev v1.1) | 160MHz 单核 RISC-V, 400KB SRAM |
| **存储配置** | 8MB SPI Flash (XMC, DIO 80MHz) | Application 分区最大 3MB，当前占用 ~744 KB |
| **显示面板** | ST7789 320×240 SPI LCD | 160×96 1bpp 逻辑帧缓冲，2x 最近邻缩放到 320×192，上下 24px 留黑居中 |
| **刷新性能** | 平均刷新耗时 **~31.5 ms** | 理论刷新率 **~31.8 fps**；单 10KB DMA Strip Buffer，零撕裂零重影 |
| **按键系统** | 3 个物理按键 (UP, DOWN, OK) | 支持单击、长按（>500ms）、双击手势，完整映射方向/确认/返回/求助 |
| **战斗内存门禁** | 64 KiB 静态连续 SRAM | `g_FightMapData` 永久驻留，杜绝战役中内存碎片或分配失败 |
| **运行栈配置** | 16,384 字节 (16 KiB) | 游戏主任务栈，硬件实测 HWM 剩余 **~14,372 字节**（实际峰值占用仅 ~2 KiB） |
| **系统可用堆** | Free Heap: **~150 KB** | Min Free: **~150 KB**, 最大连续空闲块: **~114 KB**，游玩期间零内存泄漏 |
| **存档持久化** | ESP-IDF NVS (`baye_sav`) | 支持 6 个独立存档槽位（`sango0`~`sango5`），断电/冷重启完全持久化 |
| **电量显示 (Enhanced)** | CW2017 I2C 电量计 | 右上角物理留黑区常驻极轻量显示 `[电池] XX%`，30~60s 定时更新，零整屏重绘 |
| **背景音乐 (Enhanced)** | ES8311 + I2S DMA 流式播放 | 16kHz Mono IMA ADPCM 复古战略进行曲，独立 2.5KB Worker 任务，无缝循环，与游戏及显示 DMA 零冲突 |

> **声明：** 背景音乐为 `Baye Passport Enhanced` 新增平台功能，非原版 Baye 音乐还原（原版为纯静音/蜂鸣器）。关闭 `CONFIG_BAYE_ENHANCED_MUSIC` 时完全保持原版行为与资源占用。

---

## 物理按键与操作映射

FoloToy AI Passport 配备 3 个物理按键，通过手势驱动《三国霸业》全功能操作：

| 物理按键 | 手势动作 | 映射键值 | 游戏内功能 |
| :--- | :--- | :--- | :--- |
| **UP** | 单击 (Click) | `CHAR_UP` (`0x22`) | 光标上移 / 菜单向上 |
| **UP** | 长按 (Long >500ms) | `CHAR_LEFT` (`0x24`) | 光标左移 / 战役向左 / 上一页 |
| **DOWN** | 单击 (Click) | `CHAR_DOWN` (`0x23`) | 光标下移 / 菜单向下 |
| **DOWN** | 长按 (Long >500ms) | `CHAR_RIGHT` (`0x25`) | 光标右移 / 战役向右 / 下一页 |
| **OK** | 单击 (Click) | `CHAR_ENTER` (`0x27`) | 确定 / 进入 / 下达指令 |
| **OK** | 长按 (Long >500ms) | `CHAR_EXIT` (`0x28`) | 取消 / 返回 / 退出当前菜单 |
| **OK** | 双击 (Double) | `CHAR_HELP` (`0x26`) | 帮助 / 查看城池与武将详情 |

*注：在开发模式下，还支持通过 USB-Serial/JTAG 串口终端使用键盘控制（`w`/`s`/`a`/`d`/`Enter`/`Esc`，`m` 打印内存基线，`t` 切换复古墨绿/黑白对比度主题）。*

---

## 架构与安全设计

1. **绝对安全的分区与烧录保护**
   - 固件仅烧录到 `app` 分区（偏移量 `0x10000`）。
   - **绝对禁止**烧录 `0x000000`（Bootloader）。
   - **严密保护** `cardid`（`0x356000`, 16KB）与 `recovery`（`0x700000`, 1MB）救砖分区，`tools/verify_firmware.py` 每次构建/烧录前进行哈希与边界强制拦截。
   - 保留开机长按 `UP` 键 5 秒跳转 `0x700000` 原厂恢复系统的安全机制。

2. **Fail-Closed 闭环 LCD DMA 渲染管道**
   - 废除无等待覆盖单缓冲的竞态隐患；
   - 每次提交 LCD Bitmap 后，严格等待 DMA 传输完成信号（`bsp_display_wait_trans_done`）；
   - 若遇到提交失败或 DMA 异常，立即安全中断当前帧刷新，保护 DMA 内存不被破坏。实测 `DMA Timeouts = 0`, `LCD Submit Fails = 0`。

3. **只读资源零 SRAM 消耗**
   - `dat.lib`（196,890 字节）与 `font.bin`（163,840 字节）直接通过 CMake 内嵌并由 ESP32 MMU 映射至 CPU 地址空间；
   - 16×16 点阵汉字字库与游戏资源实现零拷贝直读，占用 0 字节 SRAM。

---

## 快速上手与开发流程

### 1. 主机单元测试（无需连接硬件）
```bash
./tools/test_host.sh
```
执行帧缓冲转换、按键手势映射、虚拟文件系统与边界溢出测试。

### 2. Docker 固件构建
```bash
./tools/build_docker.sh
```
使用官方 `espressif/idf:v5.5.3` 镜像构建，并自动校验固件大小与分区安全性。

### 3. 安全烧录到设备
将 AI Passport 通过 USB 接入电脑后执行：
```bash
./tools/flash.sh /dev/cu.usbmodem101
```

### 4. 自动化真机验证与遥测
```bash
uv run --with pyserial python3 tools/test_serial_play.py /dev/cu.usbmodem101 --keys "m"
```

---

## 项目文档导航

- [系统架构全景参考 (docs/ARCHITECTURE.md)](docs/ARCHITECTURE.md)
- [硬件实测数据与基线报告 (docs/HARDWARE_VALIDATION.md)](docs/HARDWARE_VALIDATION.md)
- [平台桩函数安全审计 (docs/platform-stub-audit.md)](docs/platform-stub-audit.md)
- [音频子系统审计与未来演进 (docs/audio-audit.md)](docs/audio-audit.md)

---

## 版权与致谢

- 游戏核心逻辑来源于经典中文 RPG/SLG《三国霸业》（东莞步步高教育电子）。
- 原生 C 引擎重构参考 iBaye 项目。
- 嵌入式平台层与 BSP 专为 FoloToy AI Passport 独立移植与加固。
