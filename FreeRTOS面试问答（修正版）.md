# FreeRTOS面试问答（修正版）
## 基础题
**Q1：FreeRTOS是什么，为什么使用？**
A：面向MCU轻量实时内核；支持多任务抢占、队列、信号量互斥量、延时管理；对比裸机实时性更好；对比Linux体积小、实时确定性强。

**Q2：FreeRTOS任务四种状态？**
A：Ready就绪、Running运行、Blocked阻塞（等待延时、队列、信号量）、Suspended挂起；单核同一时刻只能有一个任务处于Running。

**Q3：FreeRTOS调度策略？**
A：默认优先级抢占，优先级数值越大优先级越高，0为空闲任务；同优先级需要打开configUSE_TIME_SLICING才开启时间片轮转；高优先级任务就绪会立刻抢占CPU。

**Q4：vTaskDelay 和 vTaskDelayUntil区别？**
A：
- vTaskDelay：相对延时，从调用时刻开始睡眠n个tick，多次执行会产生周期漂移；
- vTaskDelayUntil：绝对周期延时，以上一次唤醒时间为基准对齐，适合周期性采样任务。

**Q5：队列作用？**
A：用于任务与任务、中断与任务之间传递数据；值拷贝FIFO，支持超时；队列满/空时任务可以阻塞等待，任务间传递数据首选队列。

**Q6：二值信号量和互斥量区别？**
A：
1. 二值信号量：用于同步，无所有权，没有优先级继承；ISR中可以Give，不能Take；
2. 互斥量Mutex：保护共享资源；谁Take谁必须Give；具备优先级继承；**ISR中禁止使用**。

**Q7：什么是优先级反转，如何解决？**
A：低优先级任务拿到互斥锁；高优先级任务等待锁；中等优先级任务抢占低优先级CPU；高优先级被间接阻塞。
解决：使用FreeRTOS互斥量开启优先级继承，低优先级任务临时提升到等待锁的高优先级；释放锁后恢复原始优先级；优先级继承只能解决反转，不能解决死锁。

**Q8：ISR中如何调用FreeRTOS API，configMAX_SYSCALL_INTERRUPT_PRIORITY含义？**
A：中断内只能调用xxxFromISR接口，接口内部不能阻塞；唤醒更高优先级任务需要调用portYIELD_FROM_ISR()。
只有抢占优先级数值≥configMAX_SYSCALL_INTERRUPT_PRIORITY（逻辑优先级更低）的中断才允许调用FromISR；抢占优先级更小（逻辑优先级更高）中断不受BASEPRI屏蔽，禁止调用任何RTOS API。
> 注意：该配置**只看抢占优先级，子优先级不起作用**。

**Q9：任务栈以及栈溢出？**
A：每个任务拥有独立栈，保存寄存器上下文、局部变量、函数调用链；uxTaskGetStackHighWaterMark获取栈高水位，实际使用预留20~30%余量；打开configCHECK_FOR_STACK_OVERFLOW检测栈溢出；栈溢出典型现象HardFault、程序跑飞。

**Q10：heap1~heap5区别？**
A：
- heap_1：只分配，不释放；
- heap_2：最佳匹配分配，释放不合并空闲块；
- heap_3：简单封装标准库malloc/free，此时pvPortMalloc等价malloc；
- heap_4：最常用，最佳匹配，释放自动合并相邻空闲块，抗碎片；
- heap_5：支持多段RAM内存。
> heap1/2/4/5禁止混用裸机malloc和pvPortMalloc。

---
## 进阶原理题
**Q1：Cortex‑M一次完整任务切换流程？**
A：SysTick/Yield触发→更新tick计数，查找就绪任务→置位PendSV异常标记；退出SysTick；PendSV是最低异常优先级，执行上下文切换。
硬件自动压R0‑R3、R12、EXC_RETURN、PC、xPSR到PSP；软件汇编保存R4‑R11到当前任务栈；把PSP存入TCB；加载新任务PSP，恢复R4‑R11；异常返回硬件自动恢复剩余寄存器。

**Q2：上下文切换为什么放在PendSV，不放在SysTick？**
A：SysTick优先级较高，如果在SysTick内部做任务切换，若此时还有其他中断正在执行，会破坏中断嵌套；PendSV设置为最低异常优先级，等所有其他中断全部执行完毕之后才执行上下文切换。

**Q3：就绪表如何O(1)查找最高优先级就绪任务？**
A：每个优先级一条就绪链表，搭配uxTopReadyPriority位图；Cortex‑M使用CLZ前导零计数指令；没有CLZ指令的MCU使用查表，不需要遍历全部任务。

**Q4：临界区、关全部中断、vTaskSuspendAll三者区别？**
A：
1. taskENTER_CRITICAL：设置BASEPRI，屏蔽部分中断；带嵌套计数；临界区不能阻塞；不会关闭全部中断；
2. taskDISABLE_INTERRUPTS：关闭全部可屏蔽中断，代价大，尽量少用；
3. vTaskSuspendAll：挂起调度器，不关闭中断；中断仍然可以运行；不会发生任务之间上下文切换。

**Q5：heap_4工作原理？**
A：空闲块按地址排序链表；分配采用最佳匹配；剩余内存大于块头开销则拆分内存块；释放的时候合并前后相邻空闲块；块头部保存对齐标记、内存大小。抗碎片能力优于heap_2。

**Q6：栈溢出两种检测方式？**
A：
1. 栈尾填充魔数0xA5；任务切换时校验魔数是否被改写，检测准确；
2. 任务切换时对比PSP栈指针是否越界，速度快；
定位：栈高水位API；HardFault时读取PSP栈帧，查看PC、LR判断是否野指针。

**Q7：任务通知优点和局限？**
A：
优点：控制块就在TCB内部，不需要额外队列结构体，内存访问少，速度快；支持置位、加减、覆盖等多种通知动作，支持超时等待。
局限：允许多个生产者向同一个任务发送通知；**没有消息缓冲区，通知值容易被覆盖；只支持一个接收任务，不能做多消费者**。

**Q8：Tickless‑Idle工作流程？**
A：打开configUSE_TICKLESS_IDLE；只有空闲任务运行才会进入低功耗；计算距离最近事件的tick数；关闭SysTick，进入WFI休眠；外部中断唤醒MCU；补偿xTickCount；执行重调度。休眠期间tick延时依靠补偿恢复。

**Q9：FreeRTOS启动流程，vTaskStartScheduler为什么永不返回？**
A：硬件初始化→用户xTaskCreate创建业务任务；调用vTaskStartScheduler；内核自动创建空闲任务、软件定时器守护任务；配置PendSV为最低异常优先级，初始化SysTick；调用xPortStartScheduler；prvStartFirstTask触发SVC异常；SVC异常服务恢复第一个任务PSP与上下文，EXC_RETURN跳转到任务入口。
> main函数MSP栈没有销毁，调度器启动后不再回到main执行流，函数永不返回；SysTick**不需要配置最低优先级，只有PendSV强制最低**。

**Q10：延时链表tick溢出怎么处理？**
A：两套延时链表：普通延时链表、溢出延时链表。当任务等待时间造成tick无符号溢出，任务放入溢出链表；tick处理函数检测溢出条件交换链表指针，迁移任务，判断到期。**不是xTickCount等于0的时候交换链表**。

**Q11：队列零拷贝场景？**
A：队列尚有空闲空间，同时存在任务阻塞等待读队列；发送数据直接拷贝到接收任务内存，不走环形缓冲区；队列满直接返回errQUEUE_FULL，不会触发零拷贝。

---
## 社招高级
**Q1：单核FreeRTOS与SMP多核FreeRTOS最大区别？**
A：单核：高优先级运行时低优先级一定不会并发执行；SMP多核不同优先级任务可以跑在不同核心并行执行；单核的优先级隐含互斥不再生效，访问共享资源必须显式加锁。

**Q2：SMP临界区怎么实现？**
A：关闭本CPU本地中断 + 获取跨核自旋锁；单核版本临界区只操作BASEPRI，没有自旋锁。

**Q3：AMP和SMP区别？**
A：
SMP：多个同架构核心，共享内存，一套FreeRTOS统一调度；
AMP：每个内核独立运行固件/RTOS，可以同架构也可以异构；依靠共享内存+IPI核间中断通信。

**Q4：软件定时器回调注意事项？**
A：回调运行在Timer守护任务上下文；回调里面**禁止任何会阻塞的API，禁止vTaskDelay**；回调只做发通知，复杂业务交给普通任务处理。

**Q5：什么场景静态创建任务，什么场景动态？**
A：车规、工控、功能安全项目全部静态xTaskCreateStatic；消费类MCU核心任务静态；需要动态删除任务才使用动态创建，需要配置malloc失败钩子。

**Q6：Tickless Idle下1kHz控制环有什么问题？硬实时怎么做？**
A：Tickless依靠RTC补偿tick，会引入几十us抖动，不适合硬实时控制；硬实时闭环使用独立硬件TIM、PWM、ADC+DMA，RTOS只处理非实时业务。

**Q7：系统所有业务任务都阻塞，但是无法进入低功耗，功耗居高不下排查点？**
A：
1. 任务peek忙轮询不阻塞；
2. 软件定时器守护任务被周期定时器频繁唤醒；
3. ISR频繁调用Give；
4. Idle Hook内部有忙等；
5. 存在处于就绪态的任务，空闲任务得不到执行。

**Q8：硬件看门狗能不能放在Idle Hook喂狗？**
A：不可以。高优先级任务死循环持续占用CPU，Idle任务永远得不到运行，硬件看门狗不会喂狗，系统不会复位。硬件看门狗由独立看门狗任务喂；Idle Hook适合软件心跳统计。

**Q9：优先级反转与死锁区别？**
A：优先级反转是性能问题，任务最终可以运行；死锁任务永久卡死，互斥量优先级继承不能解决死锁。

**Q10：功能安全ISO26262使用FreeRTOS有哪些要求？**
A：使用SAFERTOS官方安全包(WITTENSTEIN)；静态分配；开启MPU内存保护；关闭动态任务删除；断言；WCET最坏执行时间分析。
