# STM32 串口命令终端 / 工业数据采集终端

基于STM32F103的数据采集终端，目标是实现 Modbus RTU 从机和 Flash 历史数据存储。

## 硬件
- STM32F103C8T6 最小系统板
- USB 转 TTL 模块
- （后续：DS18B20、W25Q64、RS485 模块）

## 已实现
- [x] UART 中断接收
- [x] 环形缓冲区（中断只写、主循环只读，无需加锁）
- [x] 按行拼接与命令解析（函数指针命令表，支持大小写、多余空格、超长命令报错）
- [ ] DS18B20 驱动
- [ ] Modbus RTU 从机
- [ ] W25Q64 历史数据存储

## 串口命令（115200, 8N1，需发送换行'\n'）
| 命令 | 说明 |
|---|---|
| LED ON / LED OFF / LED TOGGLE | 控制板载 LED（PC13，低电平亮） |
| HELP | 列出所有命令 |

## 代码结构
- ringbuf.c/.h：环形缓冲区
- cmd.c/.h：命令解析与分发
- main.c：初始化与主循环

## 踩坑
- 命令没有响应：串口助手未发送 '\n'，行结束符不存在


## 环境
vscode + Keil MDK5 + STM32CubeMX + HAL 库



## 遇到的疑问

- 为什么中断里只存数据，不处理命令？---在中断里花的时间越长，丢数据和卡死的概率越大

- 完整链路：
### 字节到达 PA10，串口硬件收到 1 个字节，触发 USART1 中断，CPU 跳到 USART1_IRQHandler，里面调用 HAL_UART_IRQHandler，HAL 把字节存进 rx_byte，再调用回调函数 HAL_UART_RxCpltCallback回调里只做两件事：rb_write 把字节放进环形缓冲区，重新开启 HAL_UART_Receive_IT（不重开就只能收 1 个字节），主循环用 rb_read 一个个取出字节，交给 cmd_feed_byte，不是 \n 的字符存进 line_buf，同时检查长度防止越界，遇到 \n，补上 \0 变成合法字符串，进入 process_command，normalize_cmd 去掉多余空格、统一大写，遍历命令表，用 strcmp 逐条比较，匹配后通过函数指针调用对应的处理函数，没匹配回 ERROR，cmd_led_on 调用 HAL_GPIO_WritePin 把 PC13 拉低，灯亮，回复 OK