# 硬件测试与 EEPROM 子菜单

主菜单新增第 10 项 **Hardware Tests** 和第 11 项 **EEPROM**。主菜单每页显示 10 项，LEFT/RIGHT 连续选择并自动换页，OK 进入，BACK 返回。所有新增诊断串口日志使用 ASCII，避免终端中文编码影响错误信息阅读。

## 开机与手动测试的关系

开机恢复 REMOTER 标识及装饰动画，动画期间进度是 0%；实际检查开始后，进度由已完成的检测项数量计算。失败不会显示 READY。启动新增 1 KiB 专用 RAM 图案测试和内部 Flash 常量读取，只能说明这些采样检查通过，不能代表全芯片内存完整性。

人工操作、仪表/对端配合和存储擦写测试放在 Hardware Tests 内按需执行。它是前台维护界面，测试期间常规菜单任务暂停；不要在控制机器人运行时进行测试。退出后恢复正常菜单。测试列表保存本次上电各项结果，取消不覆盖上次结果，不会把未测试项自动改成通过；开机汇总继续保留启动时的快照。

## 测试项目与判据

| 项目 | 操作与实际通过条件 |
|---|---|
| LCD colors / grid | 依次显示红、绿、蓝、白、黑及网格，逐页检查颜色、错位和空缺。OK 确认这一页正常，BACK 报失败。属于人工判定。 |
| Keys / digital switches | 四个菜单键各按下并松开，六路 DCH 开关/按键各经过两个稳定电平。全部检测到才通过；60 秒超时失败，长按 BACK 1 秒取消。源码 TIM4 配置不等于原理图上存在编码器，不更改 SRAM 使用的 PD12/PD13。 |
| Analog control travel | 九路控制输入分别运动到两端；每端至少连续三个样本达到 ADC 原始值 ≤410、≥3685（约 10%/90% 满量程），全部达到才通过。90 秒超时失败，BACK 取消。阈值用于全行程检查，不是精密校准。 |
| Battery meter comparison | 用电压表测实际电池电压，LEFT/RIGHT 按 10 mV 设置读数，OK 比较。按 R74/R77 的 2:1 分压、3.3 V ADC 参考及现有参数修正计算 32 次平均值，误差在 ±150 mV 内通过；不修改校准参数。 |
| LEDs / buzzer | 依次确认 LED1、LED2 和 PA15 蜂鸣器/两灯熄灭。全部由操作人员确认才通过。退出恢复 LED 输出与 PA15 原配置。 |
| UART4 cable loopback | 断开扩展外设，将 PA0(TX) 与 PA1(RX) 短接，按 OK。发送并读回 32 字节不同图案，无串口错误才通过；无回环超时失败。测试临时关闭原 RX 中断回显并清理接收状态，结束恢复；完成后去掉短接线。 |
| NRF diagnostic TX + ACK | 先启动另一块运行此固件的板子的 RX 测试，再在本机运行 TX。8 个包均实际获得 ACK 才通过；无 ACK、状态错误或超时失败。只证明测试对端链路，不代表正常机器人已连接。 |
| NRF diagnostic RX | 在本机启动后，15 秒内让测试对端发送。收到并比较正确的 8 个包才通过。BACK 取消。两端使用通道 40、1 Mbps、固定 32 字节、2 字节 CRC、自动 ACK；地址寄存器按顺序写入 `D7 43 44 47 31`。正常通信地址不会收到这些测试包。 |
| Flash dedicated-sector R/W | 先检测 Flash，再检查专用扇区全 FF；分 256 字节页写入不同图案、读回比较、擦除并检查恢复 FF。任一步失败即 FAIL。发现非 FF 为 BLOCKED，不会擦除。 |
| EEPROM reserved-byte R/W | 仅测试 AT24C08 首块的 `0xFF`，要求原值为 FF；写 55/AA 分别读回，再恢复 FF 并验证。任一步失败仍报 FAIL；原值非 FF 为 BLOCKED。 |
| SD temporary-file R/W | 使用 `FA_CREATE_NEW` 选择未占用的 `0:/D000.TMP` 至 `D999.TMP`，写入 4 KiB 图案、同步、关闭重开、完整比较、关闭并删除本次新建文件。短读写、关闭/清理失败均报 FAIL；没有空闲文件名为 BLOCKED。不会格式化、截断旧文件或测试原始扇区。 |
| Touch quality (operator) | 沿用现有画板和触摸算法，不开始自动校准。点击九个标记点，画两条斜线及断区附近竖线，实体 OK 结束绘画，然后明确确认准确、连续、无突变和双线才记录人工 PASS；实体 BACK 取消。若现有校准状态未就绪则 BLOCKED。 |
| MCU memory sample | 与开机相同，测试编译器分配的 1 KiB RAM 和 4 个内部 Flash 常量；不向固定 RAM 地址、文件系统区域写入。 |

无线测试开始前要求 FIFO 和中断状态为空，避免丢弃正常通信数据；退出时恢复频道、速率、地址、自动 ACK、动态载荷、工作模式和 CE 状态，并核对寄存器。无法恢复同样报 FAIL。

存储写入测试请保持供电。断电可能留下专用区的测试数据或新建的 TMP 文件；下次测试遇到非空专用区会停止，不自动擦除来历不明的数据。代码没有整片擦除或格式化入口。

## EEPROM 菜单与地址重叠

已确认 AT24C08 和 AT24C256 两颗均焊接。原理图标注 AT24C08 为 `0xA0`、AT24C256 为 `0xA4`，这是含读写位的 8 位地址表示。

AT24C08 的设备地址内含两个内部块选择位，A2 接低时会响应写地址 `A0/A2/A4/A6`（7 位地址 `50/51/52/53`）。因此 AT24C256 的 `A4`（7 位 `52`）与它重叠；对 AT24C256 的两字节地址操作还可能被 AT24C08 解释为地址加数据，不能靠不同长度的软件协议可靠规避。

此版本只访问 `0x50` 的首个 256 字节，菜单明确标注芯片实际容量为 1 KiB、开放范围为 256 字节，不对重叠地址扫描或读写。若要完整使用两颗器件，可在核对 PCB 后把 AT24C256 的地址硬件改到 `0xA8` 等未占用地址，再分别实现两种字地址长度的驱动。

参考：[Microchip AT24C08 数据手册](https://www.microchip.com/content/dam/mchp/documents/OTH/ProductDocuments/DataSheets/AT24C04C-AT24C08C-I2C-Compatible-Two-Wire-Serial-EEPROM-4-Kbit-8-Kbit-20006127A.pdf)、[AT24C256 数据手册](https://ww1.microchip.com/downloads/en/devicedoc/AT24C128C-AT24C256C-Data-Sheet-DS20006270B.pdf)、[Nordic nRF24L01 产品说明](https://devzone.nordicsemi.com/cfs-file/__key/support-attachments/beef5d1b77644c448dabff31668f3a47-aad1a46f307945a7b0204fd969e86bdf/content.pdf)。

EEPROM 操作步骤：

1. LEFT/RIGHT 选择地址，界面显示所在 64 字节页；`--` 表示读取失败。
2. OK 开始编辑，LEFT/RIGHT 修改高半字节，再按 OK 修改低半字节。
3. 再按 OK 进入明确的写入确认页；最后一次 OK 才真正写入，并立即读回验证。BACK 在写入前始终取消编辑。
4. `0xFF` 保留给硬件写入测试，浏览可读，编辑禁止。没有批量清空入口。

EEPROM 字节写入采用有限等待和失败复位，芯片不应答不会卡死整个菜单。读回失败或不一致明确显示 WRITE FAILED，不冒充保存成功。

## 验证方式

EIDE ARMCC5 完整编译后烧录 `VSCode EIDE Project/build/Remoter/Remoter.hex`。主机故障注入测试入口：

```text
py -3.9 7_Test/host/run_boot_tests.py D:/Embedded/mingw64/bin/gcc.exe
```

主机模拟验证软件判据、边界与失败恢复；显示效果、真实 EEPROM 装配、信号质量和无线收发仍需在板上执行菜单测试。
