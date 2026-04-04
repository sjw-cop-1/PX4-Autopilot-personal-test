# Day 17：uORB 发布订阅机制，追踪 3 条 topic 流向

## 目标
理解 uORB 是什么、如何定义、如何发布和订阅，追踪 3 条关键 topic 的数据流向。

---

## 第一步：理解数据结构定义（.msg → .h）

以下 3 个 topic 作为学习对象，覆盖传感器、位置估计、状态机三类数据：

### 1. `sensor_accel`（IMU 加速度计）
**文件：** `msg/SensorAccel.msg`

```
uint64 timestamp          # 时间戳（微秒）
uint64 timestamp_sample
uint32 device_id          # 传感器设备唯一 ID
float32 x                 # FRD 坐标系 X 轴加速度（m/s?）
float32 y
float32 z
float32 temperature
uint32 error_count
uint8[3] clip_counter
uint8 samples
uint8 ORB_QUEUE_LENGTH = 8
```

### 2. `vehicle_local_position`（EKF2 输出的本地位置）
**文件：** `msg/versioned/VehicleLocalPosition.msg`

关键字段：
- `x / y / z`：NED 坐标系位置（米）
- `vx / vy / vz`：NED 坐标系速度（m/s）
- `xy_valid / z_valid`：数据有效性标志
- `xy_reset_counter`：位置重置次数（突变检测用）

### 3. `parameter_update`（参数变更通知）
**文件：** `msg/ParameterUpdate.msg`

```
uint64 timestamp    # 通知时间戳
uint32 instance     # 单调递增的变更计数
uint32 set_count    # 本次启动以来参数被 set 的次数
uint16 changed      # 本次变更的参数数量
```

---

## 第二步：uORB 底层 API 阅读

| 文件 | 关注内容 |
|---|---|
| `platforms/common/uORB/uORB.h` | `orb_metadata` 结构体，理解 topic 的元数据（名称、大小、ID、队列长度） |
| `platforms/common/uORB/Publication.hpp` | `Publication<T>` 模板类，核心方法 `publish()` |
| `platforms/common/uORB/Subscription.hpp` | `Subscription<T>` 类，核心方法 `copy()`、`updated()` |
| `platforms/common/uORB/SubscriptionCallback.hpp` | 事件驱动回调订阅，`registerCallback()` 注册后有数据才触发 `Run()` |

### 核心概念速记

| 概念 | 说明 |
|---|---|
| `orb_advertise()` | 发布者注册，首次发布时调用 |
| `orb_publish()` | 发布一条新数据 |
| `orb_subscribe()` | 订阅者注册 |
| `orb_copy()` | 读取最新一条数据 |
| `orb_check()` | 检查是否有新数据（是否 updated） |
| `Publication<T>::publish()` | C++ 封装版本的发布 |
| `Subscription<T>::copy()` | C++ 封装版本的订阅读取 |
| `Subscription<T>::updated()` | 检查是否有新数据 |

---

## 第三步：3 条 topic 数据流向

### sensor_accel 流向
```
IMU 驱动（src/drivers/imu/）
    │  Publication<sensor_accel_s>::publish()
    ▼
[uORB 消息总线]
    │  Subscription<sensor_accel_s>::copy()
    ▼
ekf2（src/modules/ekf2/）          ← 用于状态估计
work_item_example                   ← 示例模块订阅
```

### vehicle_local_position 流向
```
ekf2（src/modules/ekf2/）
    │  Publication<vehicle_local_position_s>::publish()
    ▼
[uORB 消息总线]
    │  Subscription<vehicle_local_position_s>::copy()
    ▼
mc_pos_control（src/modules/mc_pos_control/）  ← 位置控制器
navigator（src/modules/navigator/）             ← 导航任务
```

### parameter_update 流向
```
param_set() 调用后
    │  Publication<parameter_update_s>::publish()
    ▼
[uORB 消息总线]
    │  _parameter_update_sub.updated()
    ▼
几乎所有模块（commander、ekf2、sensors 等）← 收到后调用 updateParams()
```

---

## SITL 验证命令

启动仿真后，在 PX4 shell 中执行：

```sh
# 实时查看加速度计数据
listener sensor_accel

# 实时查看位置数据
listener vehicle_local_position

# 查看所有 topic 的发布频率与订阅者数量
uorb top

# 查看特定 topic 状态
uorb status sensor_accel
uorb status vehicle_local_position
```

---

## 今日检查清单

- [ ] 阅读 `msg/SensorAccel.msg`，能解释每个字段含义
- [ ] 阅读 `msg/versioned/VehicleLocalPosition.msg`，重点看 `xy_valid` 的作用
- [ ] 阅读 `platforms/common/uORB/Publication.hpp`，找到 `publish()` 函数签名
- [ ] 阅读 `platforms/common/uORB/Subscription.hpp`，找到 `copy()` 和 `updated()` 函数签名
- [ ] 在 SITL 中执行 `uorb top`，截图记录当前 topic 频率

---

## 可验证产物

对这 3 个 topic，写出"谁发布 → 经过哪个模块 → 谁订阅"的数据流向笔记。

---

## 扩展思考（有余力时）

- `ORB_QUEUE_LENGTH = 8` 表示队列深度为 8，意味着最多缓冲 8 条未读数据。队列满后最旧的会被覆盖
- `SubscriptionCallback` vs `Subscription`：前者数据到达即触发，后者需要主动轮询 `updated()`
- uORB 的本质是共享内存 + 发布者/订阅者计数，不是网络协议
