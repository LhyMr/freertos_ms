SemaphoreHandle_t mutex1, mutex2;

void Task1(void *pvParam)
{
    while(1)
    {
        // 1. 获取 mutex1 成功，持有mutex1
        xSemaphoreTake(mutex1, portMAX_DELAY);
        // 模拟操作资源1
        vTaskDelay(pdMS_TO_TICKS(10));

        // 2. 想要获取 mutex2，但是mutex2被Task2拿着，永久阻塞
        xSemaphoreTake(mutex2, portMAX_DELAY);

        // 临界区操作
        // ...

        xSemaphoreGive(mutex2);
        xSemaphoreGive(mutex1);
    }
}

void Task2(void *pvParam)
{
    while(1)
    {
        // 1. 获取 mutex2 成功，持有mutex2
        xSemaphoreTake(mutex2, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));

        // 2. 想要获取 mutex1，但是mutex1被Task1拿着，永久阻塞
        xSemaphoreTake(mutex1, portMAX_DELAY);

        // 临界区操作
        // ...

        xSemaphoreGive(mutex1);
        xSemaphoreGive(mutex2);
    }
}


时间轴： t0        t1        t2                t3
Task1：Take(mutex1) │  Delay阻塞 │ Take(mutex2)【阻塞等mutex2】───────────▶(永久阻塞)
                    │           │      ↑(手里还握着mutex1)
Task2：             │ Take(mutex2)│  Delay阻塞  │ Take(mutex1)【阻塞等mutex1】▶(永久阻塞)
                    │            │             │      ↑(手里还握着mutex2)

资源状态：
mutex1 → Task1持有
mutex2 → Task2持有

等待环路：
Task1 ───等待──→ mutex2(Task2手里)
Task2 ───等待──→ mutex1(Task1手里)


