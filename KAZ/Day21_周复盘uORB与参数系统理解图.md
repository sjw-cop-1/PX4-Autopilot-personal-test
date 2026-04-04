# Day 21：周复盘——输出《uORB 与参数系统理解图》

## 目标
沉淀本周知识，整合 Day15–Day20 产出，建立可长期复用的参考文档。

---

## 本周产出汇总

| Day | 产出 |
|---|---|
| Day 15 | PX4 目录职责图 |
| Day 16 | rcS 启动流程图（含行号） |
| Day 17 | 3 条 topic 数据流向笔记 |
| Day 18 | 模块生命周期注释 |
| Day 19 | 参数系统完整路径说明 |
| Day 20 | 可运行的参数读取代码改动 |

---

## 整合：uORB 系统全景图

```
                    ┌─────────────────────────────────────────┐
                    │              uORB 消息总线               │
                    │  （共享内存 + 发布/订阅计数）            │
                    └─────────────────────────────────────────┘
                         ▲                      │
                         │ publish()            │ copy() / updated()
                         │                      ▼
┌──────────────┐    ┌────────────┐    ┌─────────────────────┐
│ IMU 驱动      │───?│sensor_accel│    │ ekf2                 │
│ src/drivers/ │    │ (msg 定义) │    │ src/modules/ekf2/    │
└──────────────┘    └────────────┘    └─────────────────────┘
                                               │ publish()
                                               ▼
                                    ┌──────────────────────────┐
                                    │ vehicle_local_position   │
                                    │ (msg 定义)               │
                                    └──────────────────────────┘
                                               │ copy()
                         ┌─────────────────────┴──────────────┐
                         ▼                                      ▼
              ┌─────────────────┐                   ┌──────────────────┐
              │ mc_pos_control  │                   │ navigator        │
              │ src/modules/    │                   │ src/modules/     │
              └─────────────────┘                   └──────────────────┘

param_set() ──? parameter_update ──? [所有订阅了 parameter_update 的模块]
                                      commander / ekf2 / sensors ...
                                      → updateParams() 刷新缓存
```

---

## 整合：参数系统全景图

```
1. 定义阶段（代码中）
   ─────────────────
   src/modules/commander/commander_params.c
       PARAM_DEFINE_FLOAT(TRIM_ROLL, 0.0f)

   src/examples/work_item/WorkItemExample.hpp
       DEFINE_PARAMETERS(
           (ParamInt<px4::params::COM_RC_IN_MODE>) _param_rc_in_mode
       )

2. 构建阶段（自动）
   ─────────────────
   Tools/px_process_params.py 解析注解
   生成 parameters.xml（供 QGC 显示参数说明）

3. 运行时加载（rcS）
   ─────────────────
   param select parameters.bson            ← 指定文件
   param import                            ← 加载到内存

4. 代码中读取
   ─────────────────
   param_find("TRIM_ROLL")  →  handle
   param_get(handle, &val)  →  内存缓存值

   或 C++ 方式：
   _param_rc_in_mode.get()  →  内存缓存值（需 updateParams() 刷新）

5. 修改与持久化
   ─────────────────
   param set TRIM_ROLL 0.01      ← shell 修改内存值
   param save                    ← 序列化到 parameters.bson
   系统重启 → param import 重新加载，值保留
```

---

## 本周 3 个核心问题自测

### Q1：sensor_accel 是谁发布的？
**答：** `src/drivers/imu/` 下对应的 IMU 驱动（如 `src/drivers/imu/invensense/icm42688p/`）。驱动读取硬件数据后，调用 `Publication<sensor_accel_s>::publish()` 发布到 uORB。

### Q2：vehicle_local_position 是谁发布的？
**答：** `src/modules/ekf2/`。EKF2 订阅 `sensor_accel`、`sensor_gyro`、`sensor_gps` 等多路传感器，经状态估计后发布 `vehicle_local_position`。

### Q3：commander 为什么订阅 parameter_update？
**答：** commander 需要在运行时响应参数变化（如飞行模式、超时时间等）。订阅 `parameter_update` 后，一旦有参数被修改，commander 调用 `updateParams()` 刷新内部缓存，下一个控制周期即使用新值——不需要重启模块。

---

## 本周知识点速查索引

| 知识点 | 关键文件 |
|---|---|
| 目录职责 | Day15 笔记 |
| SITL 启动流程 | `ROMFS/px4fmu_common/init.d-posix/rcS` |
| 机架参数配置 | `ROMFS/px4fmu_common/init.d-posix/airframes/10016_none_iris` |
| uORB 消息定义格式 | `msg/SensorAccel.msg`、`msg/versioned/VehicleLocalPosition.msg` |
| uORB 发布 API | `platforms/common/uORB/Publication.hpp` |
| uORB 订阅 API | `platforms/common/uORB/Subscription.hpp` |
| uORB 回调订阅 | `platforms/common/uORB/SubscriptionCallback.hpp` |
| 最简模块示例 | `src/examples/px4_simple_app/px4_simple_app.c` |
| 现代模块示例 | `src/examples/work_item/WorkItemExample.cpp` |
| 模块模板 | `src/templates/template_module/template_module.cpp` |
| 参数 C API | `src/lib/parameters/param.h` |
| 参数 C++ 宏 | `src/examples/work_item/WorkItemExample.hpp` |
| 参数定义示例 | `src/modules/commander/commander_params.c` |

---

## 下周预告（Day 22–28）

| Day | 主题 |
|---|---|
| Day 22 | commander 基础状态与 preflight 检查逻辑 |
| Day 23 | Work Queue 与任务调度基础 |
| Day 24 | 日志系统（ulog），导出一次飞行日志 |
| Day 25 | 预检失败排查（No GCS、ekf2 missing data） |
| Day 26 | Gazebo 模型加载链路（world 与 model） |
| Day 27 | x500 启动日志标注关键初始化节点 |
| Day 28 | 周复盘，提交《启动与预检排错手册 v1》 |

---

## 今日检查清单

- [ ] 整理 Day15–20 的所有笔记和产出，确认每天都有可验证产物
- [ ] 能流畅回答《3 个核心问题自测》，不查笔记
- [ ] 在 SITL 中执行 `uorb top`，能识别出本周学习的 3 个 topic
- [ ] 确认 Day20 的代码改动编译通过且在 SITL 中正常工作
- [ ] 记录本周遇到的 1–3 个困惑点，明确下周优先解答

---

## 可验证产物

一份整合上述内容的周总结文档，覆盖目录图、启动流程、uORB 全景图、参数路径 4 个模块。

---

## 节奏提示

- 今天是轻量复盘日，以整理和思考为主，不做新代码改动
- 如果某天产物未完成，今天补齐优先于做扩展阅读
- 本周核心技能：**看懂文件 → 找到关键函数 → 在 SITL 中验证**，这个闭环能力将贯穿后续所有阶段
