# Day 15：梳理目录结构与职责分工

## 目标
建立脑海中的"PX4 地图"，知道每个顶层目录负责什么。

---

## 执行步骤

打开以下目录，逐一浏览，不深入读代码，只看文件名和子目录名：

| 目录 | 职责一句话 |
|---|---|
| `src/modules/` | 飞控核心模块（commander、ekf2、mc_pos_control 等） |
| `src/drivers/` | 硬件驱动（IMU、GPS、PWM 等） |
| `src/lib/` | 共享库（参数、uORB、数学、滤波器等） |
| `src/examples/` | 官方示例（入门写模块的起点） |
| `src/systemcmds/` | 命令行工具（param、top、listener 等） |
| `src/templates/` | 模块模板 |
| `msg/` | 所有 uORB topic 的消息定义（.msg 文件） |
| `ROMFS/px4fmu_common/` | 启动脚本和机架参数 |
| `Tools/` | 辅助脚本（日志分析、仿真、参数生成等） |
| `platforms/common/uORB/` | uORB 底层实现 |
| `cmake/` | 构建系统配置 |
| `boards/` | 各硬件平台的 BSP 配置 |

---

## 关键文件定位

```
PX4-Autopilot/
├── src/
│   ├── modules/        ← 飞控逻辑（commander、ekf2 等）
│   ├── drivers/        ← 硬件驱动
│   ├── lib/            ← 公共库（parameters、uORB 客户端等）
│   ├── examples/       ← 学习写模块的起点
│   ├── systemcmds/     ← shell 命令实现
│   └── templates/      ← 新建模块时的参考模板
├── msg/                ← uORB topic 定义（*.msg → 自动生成 *.h）
├── ROMFS/px4fmu_common/
│   ├── init.d-posix/   ← SITL 启动脚本
│   └── init.d/         ← 硬件启动脚本
├── platforms/
│   └── common/uORB/    ← uORB 底层 API 实现
├── Tools/              ← 工具脚本（日志、仿真、参数）
├── cmake/              ← 构建配置
└── boards/             ← 各硬件平台 BSP
```

---

## 今日检查清单

- [ ] 打开 `src/modules/` 浏览所有模块目录名
- [ ] 打开 `msg/` 随机查看 3 个 .msg 文件，了解字段格式
- [ ] 打开 `ROMFS/px4fmu_common/init.d-posix/` 看有哪些脚本
- [ ] 打开 `platforms/common/uORB/` 看有哪些文件
- [ ] 打开 `src/examples/` 确认有 `px4_simple_app`、`work_item` 等示例

---

## 可验证产物

手写或文本版的 PX4 目录职责图，标注每个顶层目录的核心职责。

---

## 扩展思考（有余力时）

- `src/modules/` 与 `src/drivers/` 的边界：模块处理逻辑，驱动处理硬件 I/O
- `msg/` 下的 `.msg` 文件由 `msg/CMakeLists.txt` 控制，构建时自动生成对应 C/C++ 头文件到 `build/` 目录
- `boards/` 里每个硬件平台都有独立的 `CMakeLists.txt`，控制启用哪些驱动和模块
