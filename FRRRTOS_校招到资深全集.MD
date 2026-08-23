# FreeRTOS 校招到资深岗面试全集（基础 + 进阶 + 八股 + 社招压轴）
> 覆盖：概念 / 调度 / 任务 / 栈堆 / 同步通信 / 中断边界 / 定时器 / 源码级 / SMP / MPU 安全 / 实时性 / 排障 / 场景设计
> 新增补充：递归互斥、事件组、流缓冲区/消息缓冲区、死锁、任务饿死、钩子函数、任务删除坑点、返回值、常见工程坑、Cortex‑M移植坑、面试手写高频代码片段
> 适用：校招一面～社招高级岗二面

---

## 第一部分 校招基础版 10 题（概念+API 层）
1. **FreeRTOS 是什么 / 为什么用**
   面向 MCU 的轻量实时内核；多任务抢占、队列/信号量/互斥量、延时管理；比裸机实时性好、比 Linux 小且确定性强。

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
   只有**抢占优先级数值 ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY**（逻辑中断优先级更低）的中断，才允许调用`…FromISR`系列API；抢占优先级数值更小（逻辑优先级更高）的中断不受BASEPRI屏蔽，严禁调用任何FreeRTOS API。
   > 注意：该配置只看NVIC抢占优先级，子优先级不参与BASEPRI屏蔽逻辑。

9. **任务栈与溢出**
   每任务独立栈，存上下文+局部+调用链；uxTaskGetStackHighWaterMark 看高水位，留 20~30%；
   开 configCHECK_FOR_STACK_OVERFLOW；溢出典型表现 HardFault / 跑飞。

10. **heap_1~5 一句话**
    heap_4 最常用（最佳匹配+合并相邻空闲块）；heap_1 只分配不释放；heap_3 简单封装标准库malloc/free；**heap_1/2/4/5禁止混用裸机malloc与pvPortMalloc，heap_3二者等价**。

> ✨【新增基础拓展5题，面试高频】
11. **空闲任务是什么，作用？**
    优先级固定0，调度器启动自动创建，永远不会阻塞；系统无就绪任务时运行；负责回收被vTaskDelete删除任务的内核TCB与栈内存；可开启Idle Hook；**禁止在空闲任务Hook调用会阻塞的API；禁止在Idle Hook喂硬件看门狗**。

12. **任务为什么不能return？**
    任务函数是死循环；如果return，会进入`pxTaskExitError`，触发断言/HardFault；只执行一次的任务要在末尾调用`vTaskDelete(NULL)`。

13. **什么是任务饿死(Starvation)？如何避免**
    高优先级任务持续就绪不阻塞，低优先级永远得不到CPU时间；避免：高优先级任务必须设置阻塞点(delay/等IPC)，合理分配优先级，避免任务长期处于就绪态。

14. **递归互斥量什么时候用，和普通互斥区别**
    普通互斥同一任务重复Take直接死锁；递归互斥支持同一任务多次Take，内部计数，Take N次必须Give N次才释放；适合函数嵌套、库函数内部加锁场景；**不能在中断使用，不能混用普通Take/Give**。

15. **计数信号量典型场景**
    资源池、缓冲区计数、事件计数；初值代表可用资源总数；生产者Give，消费者Take。

---

## 第二部分 校招进阶版 10 题（原理链路层）
1. **Cortex‑M 一次任务切换完整链路**
   SysTick/ Yield → 更新 xTickCount、查就绪 → 挂 PendSV → 退 SysTick → PendSV（最低异常优先级）切上下文：
   硬件自动压 R0‑R3、R12、EXC_RETURN(LR)、PC、xPSR到PSP；软件手动把R4‑R11压入当前任务栈；把更新后的PSP保存到TCB；切换为新任务的PSP，恢复R4‑R11，异常返回自动恢复剩余寄存器。

2. **为什么切上下文放 PendSV 不是 SysTick**
   SysTick 优先级高，嵌套里切会乱；PendSV 最低优先级，所有 ISR 退完才切，不阻塞外设。

3. **就绪表 O(1) 找最高优先级**
   每优先级一条 ready list + uxTopReadyPriority 位图；ARM内核使用CLZ指令，无CLZ的MCU采用查表找最高置位，不遍历任务。

4. **优先级反转进阶讲法**
   Mutex 开 configUSE_PRIORITY_INHERITANCE：L 拿锁被 H 等→L 升到 H；放锁还原 uxBasePriority。
   只解反转，不解死锁，不管非 Mutex 共享。

5. **临界区 / 关中断 / 挂起调度器 三者**
   - taskENTER_CRITICAL：设置BASEPRI寄存器，屏蔽抢占优先级数值 ≤configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断，带嵌套计数，临界区内不能阻塞、禁止调用阻塞类API
   - taskDISABLE_INTERRUPTS：全关可屏蔽中断，代价大尽量少用
   - vTaskSuspendAll：只停调度不关中断，ISR 仍跑；不会发生任务上下文切换，但中断依旧可以执行

6. **configMAX_SYSCALL_INTERRUPT_PRIORITY 卡什么**
   NVIC 抢占优先级数值 ≥ 此值（Cortex‑M 数值越大逻辑优先级越低）的中断才能安全调 FromISR；
   更"高"（数值更小）的中断零延迟，BASEPRI 关不住，里面禁碰任何 RTOS 对象。
   > 注意：该配置只看NVIC抢占优先级，子优先级不参与BASEPRI屏蔽逻辑。

7. **heap_4 分配细节**
   空闲块按地址排序链表，最佳匹配，剩余 > 块头开销则拆分；释放合并前后邻块；块头对齐+size 标记。
   比 heap_2 抗碎片，比 heap_3 可控。

8. **栈溢出两种检测 + 定位**
   法1：栈尾填 0xA5 标记字，切任务查是否被改（准）；
   法2：切任务比较 PSP 是否越界（快）；
   定位：高水位 API + HardFault 时看 PSP 栈里 LR/PC 是否野。

9. **任务通知为什么快 / 局限**
   存在 TCB（ulNotifiedValue+状态位），不走队列结构体，少一次内存访问；
   支持置位/加减/覆盖/不覆盖+超时；局限：**支持多个生产者向同一个任务发送通知，但没有独立消息缓冲区，通知值容易被覆盖；仅支持一个接收任务，无法实现多消费者模型**。

10. **Tickless Idle 链路**
    configUSE_TICKLESS_IDLE 开 → 空闲任务运行时才会进入低功耗；算最近定时事件 N tick → 停 SysTick 进 WFI/STOP → 外部中断唤醒 → 补偿 xTickCount → 重调度；
    休眠期间基于 tick 的延时靠补偿恢复。

> ✨【新增进阶拓展8题】
11. **事件组EventGroup原理、限制、使用场景**
    24个有效事件位（高8位内核保留）；支持等待任意位(OR) /全部位(AND)；支持中断SetBitsFromISR；适合等待多个异步条件全部就绪；局限：**不支持超时自动清除标志；多任务同时等待同一个事件组可以，但中断不能Wait，只能Set；事件组Daemon任务处理部分逻辑，高频率触发会增加守护任务负载**。

12. **流缓冲区StreamBuffer & 消息缓冲区MessageBuffer**
    stream_buffer.c单独编译；**只支持单写者、单读者**；流缓冲区面向字节流；消息缓冲区基于流缓冲区，带消息长度头，按完整消息读写；底层复用任务通知；适合高速中断→任务、核间通信；多生产者/多消费者不能直接用，必须加锁保护。

13. **死锁四个必要条件，FreeRTOS如何规避死锁**
    四大条件：互斥、持有并等待、不可剥夺、循环等待；破坏任意一个即可；工程手段：统一锁申请顺序、申请锁设置超时、一次性申请全部资源、避免任务持有锁时阻塞等待另一个锁；**优先级继承不能解决死锁**。

14. **vTaskSuspendAll挂起调度器和临界区区别？挂起调度器里面可以调用哪些API？**
    挂起调度器：不关中断，中断仍然执行；不会发生任务切换；禁止调用任何会触发上下文切换、阻塞的API；适合多组共享变量短时保护；不能阻止中断修改共享数据。

15. **portYIELD() / portYIELD_FROM_ISR()区别**
    portYIELD：任务上下文调用，立刻触发PendSV，主动切换任务；
    portYIELD_FROM_ISR：中断内使用，仅设置PendSV挂起位，中断退出后才执行切换；**中断中绝对不能直接调用portYIELD()**。

16. **uxTaskGetStackHighWaterMark 和 uxTaskGetStackHighWaterMark2区别**
    前者返回UBaseType_t；后者返回configSTACK_DEPTH_TYPE，适配大栈深度，新版本推荐用2。高水位是历史最小剩余栈空间，越接近0越危险。

17. **Tick Hook(vApplicationTickHook)注意事项**
    在SysTick中断上下文运行；执行时间必须极短；不能调用任何会阻塞的FreeRTOS API。

18. **动态删除任务vTaskDelete坑点**
    调用vTaskDelete之后，任务句柄立即失效；**用户层不会立刻释放内存，由空闲任务运行时回收TCB与栈**；如果空闲任务永远得不到运行，内存泄露；车规安全项目禁止动态删除任务，优先静态创建。

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
12. heap_1~5：1只分配不释放 / 2最佳匹配不合并空闲块 / 3包malloc / 4合并相邻空闲块 / 5多段RAM。
13. 任务通知快；允许多生产者，但无消息缓冲容易覆盖，不支持多消费者。
14. 为什么不用关中断代替 Mutex：关中断损实时、跨任务无效、无优先级继承，无法解决优先级反转。
15. 启动流程：硬件初始化→xTaskCreate→vTaskStartScheduler（建空闲任务、软件定时器守护任务；**PendSV配置为异常最低优先级，SysTick不强制最低优先级**；初始化SysTick，起首个任务永不返回）。

> ✨【新增校招八股补充10条（面试常问坑）】
16. **递归互斥只能用xSemaphoreTakeRecursive / xSemaphoreGiveRecursive，普通Take/Give不能混用**，否则直接异常。
17. **事件组24位可用，高8位内核保留，业务不要使用高8bit**；中断只能SetBitsFromISR，不能Wait。
18. **流缓冲区、消息缓冲区默认只支持单写单读，多生产者必须加锁保护**。
19. **死锁和优先级反转区分：优先级反转是间接阻塞，死锁任务互相永久等待，优先级继承不能解决死锁**。
20. **空闲任务负责回收vTaskDelete的内存；若空闲任务永远跑不到，则内存泄露**。
21. **任务函数禁止return；只运行一次的任务末尾调用vTaskDelete(NULL)**。
22. **portYIELD_FROM_ISR参数xHigherPriorityTaskWoken是输出参数，不能直接写pdTRUE；必须传入变量地址**。
23. **configTICK_RATE_HZ越大，tick粒度越细，内核开销越大；1kHz是工程常用，100Hz适合低功耗**。
24. **临界区taskENTER_CRITICAL可以嵌套；每一次Enter必须配对Exit，嵌套计数uxCriticalNesting**。
25. **软件定时器回调函数运行在Timer守护任务上下文，回调中禁止vTaskDelay、禁止阻塞API，耗时操作要交给业务任务**。

---

## 第四部分 社招高级岗压轴 25 题（架构+源码+SMP+安全+实时+排障+场景）
### A. 架构与任务模型
1. **新项目任务划分与优先级怎么定**
   按 deadline/周期（RM：周期越短优先级越高）；控制环/保护输出最高；通信/日志最低；留空档；绝不退化成同优轮转。

2. **全局变量 vs IPC 怎么选**
   单字节标志可全局+volatile+短临界；结构/多生产者/中断‑任务边界一律队列/流缓冲/通知；安全项目禁裸全局跨模块。

3. **静态 vs 动态创建决策**
   车规/工控/认证全静态（xTaskCreateStatic）；消费 MCU 核心静态+临时对象内存池；动态删任务才用动态+开 malloc‑failed hook。

4. **软件定时器任务优先级**
   高于通信低于控制；回调运行在Timer守护任务上下文，**回调内禁止调用会阻塞的API、禁止vTaskDelay**；回调只发通知不干重活，重活甩业务任务。

> ✨A模块新增2道高级题
5. **任务饿死如何复现、如何排查**
   现象：低优先级任务永远不执行；排查看任务状态，低优先级任务长期Ready；Tracealyzer看CPU占用；解决：高优先级必须有阻塞点，合理拆分优先级，避免长循环就绪。

6. **钩子函数全套清单与约束（vApplicationXXXHook）**
   - vApplicationStackOverflowHook：栈溢出触发；不能调用耗时代码
   - vApplicationMallocFailedHook：pvPortMalloc分配失败触发，内存耗尽
   - vApplicationIdleHook：空闲任务循环调用；**禁止阻塞API，禁止喂硬件看门狗**
   - vApplicationTickHook：SysTick中断上下文；执行时间极短
   - vApplicationDaemonTaskStartupHook：软件定时器守护任务启动执行一次

### B. 源码级
5. **vTaskStartScheduler 永不返回原因**
   建空闲任务（+Timer 守护任务）→ 关中断 → 初始化全局 → xPortStartScheduler 配置 PendSV为最低异常优先级、初始化 uxCriticalNesting → prvStartFirstTask触发SVC异常，在SVC异常服务中恢复首个任务PSP与初始上下文，依靠EXC_RETURN跳转到任务入口；main函数MSP栈内存并未销毁，调度器启动后不再回到main执行流。

6. **xTaskCreate 内部动作**
   按 8 字节对齐分配栈 → 初始化 TCB：栈顶先铺初始上下文（R4‑R11、LR=pxTaskExitError、xPSR/TOP 等）使首次 PendSV 像从入口返回 → 挂就绪表 → 若调度器已跑且更高优则置 xYieldPending。

7. **延时链表 tick 回绕处理**
   维护两套延时链表：普通延时链表、溢出延时链表。当任务等待时间造成tick计数无符号溢出时，任务加入溢出链表；在tick处理中检测tick计数回绕条件，交换两套链表指针，把溢出链表任务迁移到正常延时链表，判断任务是否到期。**不是等待xTickCount等于0才交换链表**。

8. **队列尚有空间、且存在阻塞等待接收数据的任务时零拷贝**
   直接拷进接收者期望位置并唤醒，不进环形缓冲；prvCopyDataToQueue 分三条路径（有等收/有等发/正常入队）。> 注意：队列满的场景发送会直接返回errQUEUE_FULL，不会触发零拷贝。

9. **临界区嵌套计数与 BASEPRI**
   uxCriticalNesting（SMP 每核一个）；BASEPRI 只屏蔽≤阈值中断，保留更高优零延迟中断，不用 CPSID I 全关。

> ✨B模块新增源码3题
10. **SVC、PendSV、SysTick三者分工（Cortex‑M移植核心）**
    SVC：调度器启动第一次切换任务，异常；PendSV：任务上下文切换，最低优先级；SysTick：系统节拍tick，统计tick计数，唤醒延时到期任务。

11. **xYieldPending标志作用**
    中断上下文发现更高优先级任务就绪，不立刻切换，置xYieldPending；中断退出后检测该标志触发PendSV；避免在中断嵌套中执行任务切换。

12. **malloc‑failed hook触发条件**
    pvPortMalloc分配内存失败时调用；heap_1‑5都支持；动态内存耗尽排查入口；安全系统尽量避免走到这个hook，优先静态分配。

### C. SMP 与多核
10. **SMP 与单核最大语义差异**
    单核低优绝不和高优并发；SMP 多核可同时跑不同优先级，原"优先级隐含互斥"失效，必须显式锁。

11. **SMP 临界区实现**
    关本地中断 + 自旋锁；taskENTER_CRITICAL_FROM_ISR 同样拿自旋锁；单核FreeRTOS临界区仅操作BASEPRI，无自旋锁。

12. **亲和性与 configRUN_MULTIPLE_PRIORITIES**
    vTaskCoreAffinitySet 绑核；=0 时只有同优先级才多核并发，兼容老单核假设。

13. **AMP vs SMP**
    SMP：多个同架构核心，共享内存，一套FreeRTOS内核统一调度（ESP32、RP2040/Pico W）；
    AMP：每个内核独立运行自己的固件/RTOS，可以同架构也可以异构；依靠共享内存、核间中断IPI通信。

> ✨C模块新增SMP高频面试题
14. **SMP下为什么互斥量、队列依然可以直接复用，临界区增加自旋锁？**
    单核只需要屏蔽中断；多核不同CPU可同时进入临界区，必须自旋锁实现跨核互斥；FromISR在SMP下同样要拿自旋锁。

### D. MPU / 特权 / 功能安全
14. **MPU + FreeRTOS 任务**
    xTaskCreateRestricted 配 TaskParameters_t.xRegions，每任务栈/data 设 R/W/X；非特权任务越界 → MemManage Fault，不踩别人栈。

15. **ISO26262 / IEC61508 用 FreeRTOS**
    必须用官方认证包（SAFERTOS，**WITTENSTEIN**提供，IEC61508 SIL3 / ISO26262 ASIL D）+ Safety Manual + 静态分配 + MPU + 关动态删 + 断言 + WCET 分析。

16. **TrustZone 下摆放**
    内核通常在 NS 侧；Secure 侧跑 secure_heap，NS 任务经 NS 接口调安全服务；Sau/PSC 配置一致防提权。

> ✨D模块补充安全题
17. **功能安全项目为什么禁用vTaskDelete、禁用动态内存释放**
    动态删除会产生不可预测的内存碎片；空闲任务回收时机不可控；无法做WCET分析；安全系统全部静态对象。

### E. 实时性分析
17. **WCET 与调度可行性**
    单任务 WCET=指令/主频+cache miss+中断抢占叠加；RM利用率 $\sum(C_i/T_i) \le n(2^{1/n}-1)$；实际用 Tracealyzer 抓 jitter，最大响应=被抢占链 WCET 和+最长关中断+调度延迟。> n为任务数量，工程实际利用率建议控制70%以下。

18. **关中断/临界区对抖动量化**
    测最长关 BASEPRI 窗口；临界区里禁止 I2C 等 ACK 忙等；用 GPIO 翻转+trace 抓最大不可抢占窗口。

19. **Tickless 下 1kHz 控制环**
    不准（RTC 补偿 ~30us 误差）；硬实时控制环用独立 TIM+PWM+ADC DMA 闭环，RTOS 只管非实时。

> ✨E模块补充
20. **中断延迟由什么组成？**
    最大关中断时长(BASEPRI/全关中断)+内核进入中断前执行指令+中断向量跳转+ISR前置代码；优化手段：缩短临界区，尽量不用taskDISABLE_INTERRUPTS。

### F. 复杂排障
20. **偶发 HardFault 一周一次**
    HardFault_Handler 读 EXC_RETURN 判 PSP/MSP → 取栈帧 PC → map 定位；看 CFSR/BFAR；高水位反推栈溢出踩 TCB；加 canary；Tracealyzer 回放最后切换。

21. **两任务都阻塞但系统不睡（功耗下不来）**
    查忙等 peek 不阻塞 / Timer 守护被周期 Timer 唤醒 / ISR 每 tick Give / Idle Hook 里忙等；还要排查是否存在处于就绪态的任务，导致空闲任务得不到执行。

22. **队列偶发丢帧**
    队列浅+突发→加深或 ISR 用 Overwrite/环形缓冲+计数信号量；消费者慢→提优先级或拆阶段；FromISR 返回 errQUEUE_FULL 没统计→加 drop 计数。

> ✨F模块新增排障真题（大厂高频）
23. **偶发死锁如何定位？**
    手段：调试器看所有任务TCB，看每个任务阻塞在哪个IPC对象；看每个任务当前持有哪些锁；Tracealyzer看任务阻塞历史；工程：锁操作增加超时时间，记录返回值，打印任务名。

24. **pvPortMalloc返回NULL常见原因**
    堆总内存不足；内存碎片严重；heap_4多次释放分配产生大量碎片；heap_5段配置错误；中断中调用pvPortMalloc（严禁）。

25. **FromISR返回错误常见坑**
    中断抢占优先级过高（低于configMAX_SYSCALL_INTERRUPT_PRIORITY）；没有正确定义portYIELD_FROM_ISR；xHigherPriorityTaskWoken变量没有初始化为pdFALSE；直接写portYIELD()在ISR。

### G. 场景设计
23. **双核：核0 1kHz 电机环 + 核1 TCP 栈，共享参数区**
    电机环绑核0亲和，不阻塞，双缓冲 ping‑pong+自旋锁换指针；核1 收包写后台缓冲，IPI 通知核0在控制环边界安全点换指针；绝不在电机环直接读 TCP 队列。> 自旋锁只做指针交换，避免在高频控制环长时间持有锁。

24. **OTA 可热升级+断电安全**
    Bootloader 固定；APP A/B slot；下载任务中优写 SPI Flash；校验任务高优于下载低优于控制做 SHA256+签名；三段式标志 downloading/verified/need_switch；擦写用 DMA 不占 CPU。

25. **ASIL‑B 输出防任务卡死**
    独立看门狗任务（中高优）查各业务心跳（通知计次），超时进安全状态断继电器；硬件窗口看门狗+软狗双重，**硬件看门狗由看门狗任务喂狗；禁止在Idle Hook喂硬件看门狗，Idle任务仅适合软件看门狗/软件心跳统计**。

> ✨G模块新增2道场景设计面试题
26. **设计一个多条件等待：等待按键按下 OR 串口收到数据，怎么做？**
    方案1：事件组，按键ISR置位bit0；串口接收ISR置位bit1；任务xEventGroupWaitBits等待任意位。

27. **高速UART接收，中断收到字节，传给任务，选队列还是流缓冲区？**
    字节流优先流缓冲区；注意流缓冲区只支持单写单读；中断作为写者，任务作为读者。

---

## 第五部分 记忆锚点（候场前 5 分钟）
- 基础：调度看抢占、同步分清队列/Mutex/信号量、ISR 只用 FromISR、栈用高水位、反转靠 Mutex 继承
- 进阶：SysTick 打拍 → PendSV 切上下文 → BASEPRI 管临界 → PSP/TCB 存现场 → heap_4 合并邻块 → 通知走 TCB 不走队列
- 社招：单核抢占 / SMP 显式锁 / MPU 隔离 / 静态分配+安全包 / WCET 看抖动链 / PendSV 汇编存 R4‑R11 / **队列未满且有等待接收任务才会触发零拷贝**
- 新增速记：
  1. 递归互斥：Take N次必须Give N次，不能中断调用；
  2. 事件组24bit，支持AND/OR等待，中断只能SetBits；
  3. 流缓冲区单写单读，字节流；消息缓冲区带长度头；
  4. 死锁4条件，优先级继承**解决不了死锁**；
  5. vTaskDelete内存空闲任务回收，空闲跑不到就泄露；任务不能return；
  6. FromISR必须使用输出参数xHigherPriorityTaskWoken，禁止portYIELD()在中断；
  7. Idle Hook禁止阻塞API、禁止喂硬件看门狗；
- 启动：vTaskStartScheduler → 建空闲 → xPortStartScheduler 配 PendSV 最低 → SVC 起首任务 → main函数栈未销毁，调度器不再回到main执行流，函数永不返回

---

## 第六部分【新增】高频面试手写代码片段（笔试/机试口述题）
> 面试经常会口头让写，熟悉模板

### 片段1：vTaskDelayUntil绝对延时
```c
TickType_t xLastWakeTime = xTaskGetTickCount();
for(;;)
{
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    /* 周期业务 */
}


void USART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ch = USART_ReadData();
    xQueueSendFromISR(xUartQueue, &ch, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

### 片段 2：中断内队列发送 FromISR 标准模板

```
void USART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ch = USART_ReadData();
    xQueueSendFromISR(xUartQueue, &ch, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### 片段 3：递归互斥示例

```
xSemaphoreTakeRecursive(xRecMutex, portMAX_DELAY);
{
    xSemaphoreTakeRecursive(xRecMutex, portMAX_DELAY);
    /* 嵌套操作 */
    xSemaphoreGiveRecursive(xRecMutex);
}
xSemaphoreGiveRecursive(xRecMutex);
```

### 片段 4：事件组等待多个条件全部就绪

```
EventBits_t bits = xEventGroupWaitBits(xEvent,
        BIT0 | BIT1,
        pdTRUE,   /*退出清除标志*/
        pdTRUE,   /*等待全部位(AND)*/
        portMAX_DELAY);
```

### 片段 5：任务通知当作二值信号量（轻量同步）

```
/*接收任务*/
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

/*发送方（任务/中断）*/
xTaskNotifyGive(xTaskHandle);
```

### 片段 6：空闲钩子函数模板

```
void vApplicationIdleHook(void)
{
    /*禁止调用任何阻塞FreeRTOS API，禁止喂硬件看门狗*/
}
```

## 附：官方参考

- FreeRTOS 官网内核 / 特性：[https://www.freertos.org/index.html](https://www.freertos.org/index.html)
- 安全认证（SAFERTOS / WITTENSTEIN / ISO26262 ASIL D）：[https://freertos.org/Partners/Safety](https://freertos.org/Partners/Safety)