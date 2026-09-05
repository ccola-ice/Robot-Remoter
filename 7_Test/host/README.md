# 触摸主机回归测试

在 Windows PowerShell 中运行（需要 MinGW GCC）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\7_Test\host\run_touch_tests.ps1
```

从 EIDE 工程目录运行时，将路径改为 `..\7_Test\host\run_touch_tests.ps1`。
可使用 `-Compiler 'D:\path\to\gcc.exe'` 指定编译器。

测试直接编译生产代码 `gt9xx.c` 和 `palette.c`，用模拟寄存器总线与帧缓冲替代硬件。
覆盖坐标方向和连续性、工厂分辨率缩放、延迟就绪与漏中断、五指记录换序、
无效/重复 ID、部分读取失败、状态确认失败、释放超时和计时回绕、按钮归属与边界、
全部笔宽和橡皮擦的四边裁剪、清屏恢复颜色、双地址探测及初始化失败后的禁用状态。

新增四角校准回归：八种 LCD 扫描模式、镜像、交换轴且超出名义范围、X/Y 半周期回绕、
在原始 799/0 处抖动的平均值、穿过中线的逐像素连续笔迹、无效采样和不一致的四角数据拒绝。
`test_field_calibration()` 重放用户 FIELD-CAL-2 日志的四角坐标，验证选择 X 回绕模型，
四角最大残差不超过 24 像素，第 4 点后进入画板，中心双触点不再触发校准拒绝。
独立测试直接编译串口文本转换头文件及生产 `cc936.c`，检查 GBK 中文转 UTF-8、
ASCII、CRLF 和非法字节后的恢复。

这些文件不加入 EIDE 固件目标。测试不会访问串口、下载器或实际外设。
它不能验证电气时序、排线实物接法、屏幕贴合方向以及运行时采样速度。
