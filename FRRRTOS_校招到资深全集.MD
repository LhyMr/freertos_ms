# FreeRTOS 校招到资深岗面试全集（基础 + 进阶 + 八股 + 社招压轴）

> 覆盖：概念 / 调度 / 任务 / 栈堆 / 同步通信 / 中断边界 / 定时器 / 源码级 / SMP / MPU 安全 / 实时性 / 排障 / 场景设计
> 适用：校招一面～社招高级岗二面

---

## 第一部分 校招基础版 10 题（概念+API 层）

1. **FreeRTOS 是什么 / 为什么用**
   面向 MCU 的轻量实时内核；多任务抢占、队列/信号量/互斥量、延时管理；比裸机实时性好、比 Linux 小且确定性强。[1](@ref)

2. **任务四种状态**
   Ready / Running / Blocked（等延时、等队列、等信号量）/ Suspended（vTaskSuspend 挂起）。单核同时只有一个 Running。

3. **调度策略**
   默认优先级抢占（数值越大越高，0 是空闲任务）；同优先级开 configUSE_TIME_SLICING 才时间片轮转；高优先级就绪立即抢占。

4. **vTaskDelay vs vTaskDelayUntil**
   vTaskDelay(n)：相对延时，从调用起睡 n tick，周期漂移；
   vTaskDelayUntil(&last, n)：绝对延时，按上次唤醒对齐，适合周期采样。

5. **队列干什么**
   任务间/中断与任务间传数据，值拷贝、FIFO、可超时；满/空则阻塞。传数据首选队列。

6. **二值信号量 vs 互斥量（必背）**
   二值信号量：同步、无所有权、无优先级继承、不在 ISR Take；
   互斥量：保护共享资源、谁 Take 谁 Give、带优先级继承、不能在 ISR 用。

7. **优先级反转是什么 / 怎么解**
   低 L 拿锁→高 H 等锁→中 M 抢 L 的 CPU→H 被 M 间接阻塞；
   Mutex 优先级继承（L 临时升到 H，放锁还原）；不解决死锁。

8. **ISR 里能用什么 API**
   只能 ...FromISR 系列（xQueueSendFromISR / xSemaphoreGiveFromISR / xTaskNotifyFromISR），不能阻塞；
   唤醒更高优任务则 portYIELD_FROM_ISR(xHigherPriorityTaskWoken)；
   调 RTOS API 的中断 NVIC 优先级必须 ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY。

9. **任务栈与溢出**
   每任务独立栈，存上下文+局部+调用链；uxTaskGetStackHighWaterMark 看高水位，留 20~30%；
   开 configCHECK_FOR_STACK_OVERFLOW；溢出典型表现 HardFault / 跑飞。

10. **heap_1~5 一句话**
    heap_4 最常用（最佳匹配+合并相邻）；heap_1 只分配不释放；heap_3 包 malloc/free；别混用裸 malloc 和 pvPortMalloc。

---

## 第二部分 校招进阶版 10 题（原理链路层）

1. **Cortex-M 一次任务切换完整链路**
   SysTick/ Yield → 更新 xTickCount、查就绪 → 挂 PendSV → 退 SysTick → PendSV（最低异常优先级）切上下文：
   硬件自动压 R0-R3/R12/LR/PC/xPSR，软件存 R4-R11 到当前栈，TCB 存 PSP，新任务恢复 PSP+寄存器。[8,11](@ref)

2. **为什么切上下文放 PendSV 不是 SysTick**
   SysTick 优先级高，嵌套里切会乱；PendSV 最低优先级，所有 ISR 退完才切，不阻塞外设。[13](@ref)

3. **就绪表 O(1) 找最高优先级**
   每优先级一条 ready list + uxTopReadyPriority 位图；CLZ 或查表找最高置位，不遍历任务。

4. **优先级反转进阶讲法**
   Mutex 开 configUSE_PRIORITY_INHERITANCE：L 拿锁被 H 等→L 升到 H；放锁还原 uxBasePriority。
   只解反转，不解死锁，不管非 Mutex 共享。

5. **临界区 / 关中断 / 挂起调度器 三者**
   - taskENTER_CRITICAL：设 BASEPRI 屏蔽 ≤configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断，有嵌套计数，不能阻塞
   - taskDISABLE_INTERRUPTS：全关可屏蔽中断，重
   - vTaskSuspendAll：只停调度不关中断，ISR 仍跑

6. **configMAX_SYSCALL_INTERRUPT_PRIORITY 卡什么**
   NVIC 抢占优先级数值 ≥ 此值（Cortex-M 数值越大逻辑优先级越低）的中断才能安全调 FromISR；
   更"高"（数值更小）的中断零延迟，BASEPRI 关不住，里面禁碰任何 RTOS 对象。

7. **heap_4 分配细节**
   空闲块按地址排序链表，最佳匹配，剩余 > 块头开销则拆分；释放合并前后邻块；块头对齐+size 标记。
   比 heap_2 抗碎片，比 heap_3 可控。

8. **栈溢出两种检测 + 定位**
   法1：栈尾填 0xA5 标记字，切任务查是否被改（准）；
   法2：切任务比较 PSP 是否越界（快）；
   定位：高水位 API + HardFault 时看 PSP 栈里 LR/PC 是否野。

9. **任务通知为什么快 / 局限**
   存在 TCB（ulNotifiedValue+状态位），不走队列结构体，少一次内存访问；
   支持置位/加减/覆盖/不覆盖+超时；局限：只能 1:1，不能多生产者多消费者缓冲。

10. **Tickless Idle 链路**
    configUSE_TICKLESS_IDLE 开 → 算最近定时事件 N tick → 停 SysTick 进 WFI/STOP → 外部中断唤醒 → 补偿 xTickCount → 重调度；
    休眠期间基于 tick 的延时靠补偿恢复。

---

## 第三部分 校招八股文 15 条（考官原话接法）

1. 裸机 vs FreeRTOS：单执行流轮询 vs 多任务抢占+IPC+栈隔离。
2. 任务状态转移：创建→Ready；调度→Running；delay/等IPC→Blocked；Suspend→Suspended；删→Deleted。
3. 同优先级默认不轮转，开 configUSE_TIME_SLICING 才按 SysTick 轮。
4. vTaskDelay 相对、vTaskDelayUntil 绝对。
5. 传数据→队列；同步→二值信号量；N 资源→计数信号量；共享资源→Mutex。
6. Mutex vs 二值信号量：所有权+优先级继承 vs 无。
7. 优先级反转：L 拿锁 H 等 M 跑；解法是 Mutex 继承。
8. ISR 只能 FromISR，不能阻塞，退出前 portYIELD_FROM_ISR。
9. ISR 禁普通 API：普通 API 可能阻塞/切任务，破坏内核态。
10. 任务不跑查：调度器没起 / 优先级饿死 / 无阻塞点 / 栈溢出 / 句柄 NULL / NVIC 配错屏蔽 SysTick。
11. 栈溢出检测：configCHECK_FOR_STACK_OVERFLOW=1 或 2 + 高水位。
12. heap_1~5：1只分配不释放 / 2最佳不匹配不合并 / 3包malloc / 4合并相邻 / 5多段RAM。
13. 任务通知快但只能 1:1。
14. 为什么不用关中断代替 Mutex：关中断损实时、跨任务无效、无继承。
15. 启动流程：硬件初始化→xTaskCreate→vTaskStartScheduler（建空闲任务、配 SysTick/PendSV 最低优、起首个任务永不返回）。[5,15](@ref)

---

## 第四部分 社招高级岗压轴 25 题（架构+源码+SMP+安全+实时+排障+场景）

### A. 架构与任务模型
1. **新项目任务划分与优先级怎么定**
   按 deadline/周期（RM：周期越短优先级越高）；控制环/保护输出最高；通信/日志最低；留空档；绝不退化成同优轮转。
2. **全局变量 vs IPC 怎么选**
   单字节标志可全局+volatile+短临界；结构/多生产者/中断-任务边界一律队列/流缓冲/通知；安全项目禁裸全局跨模块。
3. **静态 vs 动态创建决策**
   车规/工控/认证全静态（xTaskCreateStatic）；消费 MCU 核心静态+临时对象内存池；动态删任务才用动态+开 malloc-failed hook。
4. **软件定时器任务优先级**
   高于通信低于控制；回调只发通知不干重活，重活甩业务任务。

### B. 源码级
5. **vTaskStartScheduler 永不返回原因**
   建空闲任务（+Timer 守护任务）→ 关中断 → 初始化全局 → xPortStartScheduler 配 PendSV/SysTick 最低优、初始化 uxCriticalNesting → prvStartFirstTask 用 SVC/伪造 EXC_RETURN 切到首个任务，main 的原线程上下文被丢弃。[5,15](@ref)
6. **xTaskCreate 内部动作**
   按 8 字节对齐分配栈 → 初始化 TCB：栈顶先铺初始上下文（R4-R11、LR=pxTaskExitError、xPSR/TOP 等）使首次 PendSV 像从入口返回 → 挂就绪表 → 若调度器已跑且更高优则置 xYieldPending。
7. **延时链表 tick 回绕处理**
   两张延时链表（当前/溢出）切换；xTaskIncrementTick 检测 xTickCount 回 0 时交换，把溢出部分条目迁回。
8. **队列满但有接收者阻塞时零拷贝**
   直接拷进接收者期望位置并唤醒，不进环形缓冲；prvCopyDataToQueue 分三条路径（有等收/有等发/正常入队）。
9. **临界区嵌套计数与 BASEPRI**
   uxCriticalNesting（SMP 每核一个）；BASEPRI 只屏蔽≤阈值中断，保留更高优零延迟中断，不用 CPSID I 全关。[11](@ref)

### C. SMP 与多核
10. **SMP 与单核最大语义差异**
    单核低优绝不和高优并发；SMP 多核可同时跑不同优先级，原"优先级隐含互斥"失效，必须显式锁。
11. **SMP 临界区实现**
    关本地中断 + 自旋锁；taskENTER_CRITICAL_FROM_ISR 同样拿自旋锁。
12. **亲和性与 configRUN_MULTIPLE_PRIORITIES**
    vTaskCoreAffinitySet 绑核；=0 时只有同优先级才多核并发，兼容老单核假设。
13. **AMP vs SMP**
    SMP 同架构共享 RAM 统一调度（ESP32/Pico W）；AMP 异构/信任边界各自调度，核间共享内存+IPI。

### D. MPU / 特权 / 功能安全
14. **MPU + FreeRTOS 任务**
    xTaskCreateRestricted 配 TaskParameters_t.xRegions，每任务栈/data 设 R/W/X；非特权任务越界 → MemManage Fault，不踩别人栈。
15. **ISO26262 / IEC61508 用 FreeRTOS**
    必须用官方认证包（SAFERTOS 衍生，WHIS 提供，IEC61508 SIL3 / ISO26262 ASIL D）+ Safety Manual + 静态分配 + MPU + 关动态删 + 断言 + WCET 分析。[3,6](@ref)
16. **TrustZone 下摆放**
    内核通常在 NS 侧；Secure 侧跑 secure_heap，NS 任务经 NS 接口调安全服务；Sau/PSC 配置一致防提权。

### E. 实时性分析
17. **WCET 与调度可行性**
    单任务 WCET=指令/主频+cache miss+中断抢占叠加；RM 利用率 Σ(Ci/Ti)≤n(2^(1/n)-1)；实际用 Tracealyzer 抓 jitter，最大响应=被抢占链 WCET 和+最长关中断+调度延迟。
18. **关中断/临界区对抖动量化**
    测最长关 BASEPRI 窗口；临界区里禁止 I2C 等 ACK 忙等；用 GPIO 翻转+trace 抓最大不可抢占窗口。
19. **Tickless 下 1kHz 控制环**
    不准（RTC 补偿 ~30us 误差）；硬实时控制环用独立 TIM+PWM+ADC DMA 闭环，RTOS 只管非实时。

### F. 复杂排障
20. **偶发 HardFault 一周一次**
    HardFault_Handler 读 EXC_RETURN 判 PSP/MSP → 取栈帧 PC → map 定位；看 CFSR/BFAR；高水位反推栈溢出踩 TCB；加 canary；Tracealyzer 回放最后切换。
21. **两任务都阻塞但系统不睡（功耗下不来）**
    查忙等 peek 不阻塞 / Timer 守护被周期 Timer 唤醒 / ISR 每 tick Give / Idle Hook 里忙等。
22. **队列偶发丢帧**
    队列浅+突发→加深或 ISR 用 Overwrite/环形缓冲+计数信号量；消费者慢→提优先级或拆阶段；FromISR 返回 errQUEUE_FULL 没统计→加 drop 计数。

### G. 场景设计
23. **双核：核0 1kHz 电机环 + 核1 TCP 栈，共享参数区**
    电机环绑核0亲和，不阻塞，双缓冲 ping-pong+自旋锁换指针；核1 收包写后台缓冲，IPI 通知核0在控制环边界安全点换指针；绝不在电机环直接读 TCP 队列。
24. **OTA 可热升级+断电安全**
    Bootloader 固定；APP A/B slot；下载任务中优写 SPI Flash；校验任务高优于下载低优于控制做 SHA256+签名；三段式标志 downloading/verified/need_switch；擦写用 DMA 不占 CPU。
25. **ASIL-B 输出防任务卡死**
    独立看门狗任务（中高优）查各业务心跳（通知计次），超时进安全状态断继电器；硬件窗口看门狗+软狗双重，Idle 喂软狗、看门狗任务喂硬狗。

---

## 第五部分 记忆锚点（候场前 5 分钟）

- 基础：调度看抢占、同步分清队列/Mutex/信号量、ISR 只用 FromISR、栈用高水位、反转靠 Mutex 继承
- 进阶：SysTick 打拍 → PendSV 切上下文 → BASEPRI 管临界 → PSP/TCB 存现场 → heap_4 合并邻块 → 通知走 TCB 不走队列
- 社招：单核抢占 / SMP 显式锁 / MPU 隔离 / 静态分配+安全包 / WCET 看抖动链 / PendSV 汇编存 R4-R11 / 队列满有等待者零拷贝
- 启动：vTaskStartScheduler → 建空闲 → xPortStartScheduler 配 PendSV 最低 → SVC 起首任务 → main 上下文丢弃永不返回[5,15](@ref)

---

## 附：官方参考
- FreeRTOS 官网内核/特性：https://www.freertos.org/index.html [1](@ref)
- 安全认证（SAFERTOS / WITTENSTEIN / ISO26262 ASIL D）：https://freertos.org/Partners/Safety [3](@ref)