#include "control_link.h"
#include "control_safety.h"
#include "channel_input.h"
#include "robot_control_protocol.h"
#include "app_config.h"
#include "bsp_spi_nrf.h"
#include "bsp_SysTick.h"
#include "bsp_gpio_digital_channel.h"
#include "bsp_adc1_independent_dual.h"

/* 控制链路由前台持续驱动：采样和组帧按固定节拍运行，无线发送通过轮询推进。
 * 本模块维护本机安全状态与发送统计，不把无线 ACK 当作机器人执行反馈。 */
extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
static ControlSafety safety;
/* last_service 记录最近一次控制采样，last_ack 记录最近一次硬件发送成功。 */
static uint32_t last_send, last_service, last_ack;
/* 配置代次变化可识别无线重配置，使旧配置下获得的链路状态及时失效。 */
static uint32_t radio_generation;
static uint16_t sequence;
static uint8_t boot_ok, busy, ack_seen;
static const char *status_text = "BOOT LOCK";
static ControlLinkSnapshot monitor;
static uint32_t monitor_sample_ms, monitor_tx_ms;

/* 按指定通道的校准、微调和反向设置归一化；channel 必须是有效的参数数组索引。 */
static int16_t control_axis(uint16_t raw, uint8_t channel)
{
    return channel_input_normalize(raw, param.chLower[channel],
        param.chMiddle[channel], param.chUpper[channel],
        param.PWMadjustValue[channel], param.chReverse[channel]);
}

/* 清空使能流程和监视统计，并记住关键启动检查是否通过。
 * boot_permitted 为真仅解除启动门槛，仍需有效输入、ACK 和人工使能才能发运动指令。 */
void control_link_init(uint8_t boot_permitted)
{
    unsigned long now;
    get_tick_count(&now);
    memset(&safety, 0, sizeof(safety));
    memset(&monitor, 0, sizeof(monitor));
    status_text = "BOOT LOCK";
    last_send = last_service = (uint32_t)now;
    sequence = 0U; busy = ack_seen = 0U; boot_ok = boot_permitted;
    radio_generation = NRF_GetConfigGeneration();
}

/* 返回最近一次控制周期选择的状态文本，不在查询时重新采样或访问无线硬件。 */
const char *control_link_status(void) { return status_text; }

/* 将毫秒年龄压缩到界面使用的 16 位范围；未发生过的事件与超长年龄统一显示为 65535。 */
static uint16_t control_monitor_age(uint32_t now, uint32_t then, uint8_t valid)
{
    uint32_t age = now - then;
    return !valid || age > 65535UL ? 65535U : (uint16_t)age;
}

/* 复制前台维护的最近状态，并按查询时刻更新各事件年龄；空指针不执行操作。
 * 采样标志和输入有效性分开保存，因此界面可展示故障发生时读到的原始值。 */
void control_link_get_snapshot(ControlLinkSnapshot *snapshot)
{
    unsigned long now;
    if(!snapshot) return;
    get_tick_count(&now);
    *snapshot = monitor;
    snapshot->sample_age_ms = control_monitor_age((uint32_t)now, monitor_sample_ms, monitor.sampled);
    snapshot->tx_age_ms = control_monitor_age((uint32_t)now, monitor_tx_ms, monitor.sent);
    snapshot->ack_age_ms = control_monitor_age((uint32_t)now, last_ack, ack_seen);
    snapshot->ack_seen = ack_seen;
}

/* 撤销运动使能并取消在途发送；该操作本身不发包，后续服务周期会按条件发送停止帧。 */
void control_link_inhibit(void)
{
    /* 同时清除使能条件和历史 ACK；恢复后需重新确认链路并完成摇杆回中流程。 */
    memset(&safety, 0, sizeof(safety));
    ack_seen = 0U;
    if(busy) NRF_TxCancel();
    busy = 0U;
}

/* 由主循环和耗时操作的后台回调频繁调用，内部限制为每 20 ms 尝试发送一帧。
 * control_page 表示当前是否处于机器人控制页；离页或服务间隔过长都会重置使能。
 * 只维护本机发送状态，不接收或聚合 RT 遥测，也不允许多处并发调用。 */
void control_link_service(uint8_t control_page)
{
    unsigned long tick;
    uint32_t now, gap;
    uint8_t result, channel, fresh, healthy, pressed, neutral, link_ok;
    uint16_t sample[NUM_OF_ADC1CHANNEL];
    float voltage;
    uint8_t packet[ROBOT_PACKET_SIZE];
    RobotControlCommand command;
    get_tick_count(&tick); now = (uint32_t)tick;
    /* 必须先记录链路失效，再轮询新恢复的 ACK。延迟到达的成功结果可以
     * 恢复链路，但不得保留上一次的运动使能状态。 */
    if(ack_seen && (uint32_t)(now - last_ack) >= 150U)
        control_link_inhibit();
    /* 无线配置变更后，原频道或模式下取得的 ACK 不再作为当前链路有效的依据。 */
    if(radio_generation != NRF_GetConfigGeneration()) {
        control_link_inhibit();
        radio_generation = NRF_GetConfigGeneration();
    }
    if(!param.NRF_Mode) control_link_inhibit();
    if(!control_page || (uint32_t)(now - last_service) > 100U)
        memset(&safety, 0, sizeof(safety));
    /* 每轮先轮询异步发送结果，再按 20 ms 节拍采样并发起下一帧，避免忙等无线硬件。 */
    if(busy) {
        result = NRF_TxPoll();
        if(result != NRF_TX_PENDING) {
            busy = 0U;
            if(result == TX_DS) { last_ack = now; ack_seen = 1U; monitor.tx_acked++; }
            else monitor.tx_failed++;
        }
    }
    if((uint32_t)(now - last_send) < 20U) return;
    /* 延迟后从当前时刻重新计时，不追赶积压周期，避免短时间连续发送旧输入。 */
    gap = now - last_service; last_service = last_send = now;
    /* 完成标志在消费后清除；下一轮必须再次完成 DMA 传输，输入才可视为新鲜。 */
    fresh = DMA_GetFlagStatus(DUAL_ADC1_DMA_STREAM, DMA_FLAG_TCIF4) != RESET;
    if(DMA_GetFlagStatus(DUAL_ADC1_DMA_STREAM,
       DMA_FLAG_TEIF4 | DMA_FLAG_DMEIF4 | DMA_FLAG_FEIF4) != RESET ||
       !(DUAL_ADC1_DMA_STREAM->CR & DMA_SxCR_EN)) fresh = 0U;
    DMA_ClearFlag(DUAL_ADC1_DMA_STREAM, DMA_FLAG_TCIF4);
    for(channel = 0U; channel < NUM_OF_ADC1CHANNEL; channel++) {
        sample[channel] = ADC1_Value[channel];
        if(sample[channel] > 4095U) fresh = 0U;
    }
    monitor.sampled = 1U;
    monitor.input_fresh = fresh;
    monitor_sample_ms = now;
    for(channel = 0U; channel < 6U; channel++) {
        monitor.raw[channel] = sample[channel];
        monitor.calibrated[channel] = control_axis(sample[channel], channel);
    }
    memset(&command, 0, sizeof(command));
    /* 按机器人坐标方向映射摇杆，航向由千分比换算为 0.1 度，范围为 ±180 度。 */
    command.x = -control_axis(sample[2], 2U);
    command.y = control_axis(sample[1], 1U);
    command.heading = (int16_t)(-control_axis(sample[5], 5U) * 18L / 10L);
    command.limit = 1000U;
    for(channel = 0U; channel < DIGITAL_CHANNEL_COUNT; channel++)
        if(digital_channel_get_stable(channel) == 0U)
            command.digital |= (uint16_t)(1U << channel);
    /* 松开按键时立即按物理输入撤销使能；按下按键时仍需消抖确认。 */
    pressed = ((command.digital & 1U) != 0U &&
               GPIO_ReadInputDataBit(DCH1_GPIO_PORT, DCH1_GPIO_PIN) == Bit_RESET);
    if(!pressed) command.digital &= (uint16_t)~1U;
    neutral = !command.x && !command.y && !command.heading;
    /* ADC 第七路为本机电池电压；batVoltAdjust 按千分比修正，不是机器人电池遥测。 */
    voltage = (float)sample[6] * (6.6f / 4095.0f) * (float)param.batVoltAdjust / 1000.0f;
    link_ok = ack_seen && (uint32_t)(now - last_ack) < 150U;
    /* 健康条件只允许进入使能流程，实际使能还需满足松键、回中等待和再次按键。 */
    healthy = boot_ok && fresh && control_page && param.NRF_Mode &&
              gap <= 100U && link_ok && voltage >= param.warnBatVolt &&
              !NRF_GetIoError();
    command.armed = control_safety_step(&safety, now, healthy, pressed, neutral);
    /* 多项条件同时失效时按此顺序显示首要原因，状态文本不会替代上面的完整使能判定。 */
    if(!boot_ok) status_text = "BOOT LOCK";
    else if(!param.NRF_Mode) status_text = "RADIO OFF";
    else if(!fresh || gap > 100U) status_text = "INPUT STALE";
    else if(voltage < param.warnBatVolt) status_text = "LOW TX BATTERY";
    else if(!control_page) status_text = "STOP / OPEN CONTROL PAGE";
    else if(!link_ok || NRF_GetIoError()) status_text = "NO RADIO ACK";
    else if(command.armed) status_text = "ENABLED / HOLD DCH1";
    else if(safety.ready) status_text = "READY / PRESS DCH1";
    else status_text = "RELEASE DCH1 / CENTER STICKS";
    /* 未使能时仍发送零运动控制帧，以获取 ACK 并让接收端及时得知停止状态。 */
    if(param.NRF_Mode && !busy && !NRF_GetIoError()) {
        command.sequence = sequence++;
        robot_packet_encode(packet, &command);
        busy = NRF_TxStart(packet);
        if(busy) {
            /* 解码已经编码的数据包：未使能时，线上传输的控制值全部为零。 */
            (void)robot_packet_decode(packet, &monitor.transmitted);
            monitor.sent = 1U;
            monitor_tx_ms = now;
            monitor.tx_started++;
        }
    }
}
