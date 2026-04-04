# Day 18：阅读示例模块并标注主入口执行路径

## 目标
理解一个 PX4 模块的完整生命周期：注册 → 启动 → 运行 → 退出。

---

## 主要文件

| 文件 | 关注点 |
|---|---|
| `src/examples/px4_simple_app/px4_simple_app.c` | 最简模块，入口 `px4_simple_app_main()`，适合第一次理解模块结构 |
| `src/examples/work_item/WorkItemExample.hpp` | 现代模块写法，继承 `ModuleBase + ModuleParams + ScheduledWorkItem` |
| `src/examples/work_item/WorkItemExample.cpp` | 核心逻辑：`init()` 注册回调，`Run()` 周期执行，参数更新检测 |
| `src/templates/template_module/template_module.h` | 标准模板头文件，看类继承与 `DEFINE_PARAMETERS` 宏 |
| `src/templates/template_module/template_module.cpp` | 模板实现，含 `task_spawn`、`custom_command`、`print_usage` |

---

## 第一阶段：最简示例 px4_simple_app

**文件：** `src/examples/px4_simple_app/px4_simple_app.c`

```c
// 核心仅 3 行
__EXPORT int px4_simple_app_main(int argc, char *argv[])
{
    PX4_INFO("Hello Sky!");
    return OK;
}
```

要点：
- `__EXPORT` 宏使函数在 NuttX 上从动态库导出
- `PX4_INFO()` 是 PX4 日志宏，对应 `[INFO]` 级别
- 这个模块没有循环，执行一次即退出

---

## 第二阶段：现代模块写法（WorkItemExample）

### 类继承关系

```
WorkItemExample
    ├─ ModuleBase<WorkItemExample>     ← 提供 start/stop/status 框架
    ├─ ModuleParams                    ← 提供 DEFINE_PARAMETERS 与 updateParams()
    └─ px4::ScheduledWorkItem         ← 提供调度能力（周期或回调）
```

### init() — 模块初始化

**文件：** `src/examples/work_item/WorkItemExample.cpp`

```cpp
bool WorkItemExample::init()
{
    // 注册 sensor_accel 数据到达时触发 Run()
    if (!_sensor_accel_sub.registerCallback()) {
        PX4_ERR("callback registration failed");
        return false;
    }
    // 也可以改为固定周期调度：
    // ScheduleOnInterval(5000_us); // 200 Hz
    return true;
}
```

### Run() — 核心执行逻辑

```cpp
void WorkItemExample::Run()
{
    if (should_exit()) {
        ScheduleClear();
        exit_and_cleanup();
        return;
    }

    // 检查参数是否有更新
    if (_parameter_update_sub.updated()) {
        parameter_update_s param_update;
        _parameter_update_sub.copy(&param_update);
        updateParams();   // 自动刷新 DEFINE_PARAMETERS 中的所有参数
    }

    // 读取传感器数据（触发本次 Run 的 topic）
    sensor_accel_s accel{};
    if (_sensor_accel_sub.copy(&accel)) {
        // 处理 accel.x / accel.y / accel.z
    }
}
```

---

## 模块完整生命周期

```
shell 命令: work_item_example start
        │
        ▼
ModuleBase::main()
        │
        ▼
task_spawn()                          ← template_module.cpp ~L76
        │  px4_task_spawn_cmd()        ← 创建操作系统任务线程
        ▼
run_trampoline()
        │
        ▼
instantiate()                         ← new WorkItemExample()
        │
        ▼
init()                                ← 注册 callback 或 ScheduleOnInterval
        │
        ▼ （每次有新数据或定时器触发）
Run()
        │
        ├─ should_exit()? ──Yes──? ScheduleClear() → exit_and_cleanup()
        │
        ├─ 检查 parameter_update
        │
        └─ 处理业务逻辑（读 topic、发布数据等）
```

---

## task_spawn 核心代码（template_module.cpp）

```cpp
int TemplateModule::task_spawn(int argc, char *argv[])
{
    _task_id = px4_task_spawn_cmd(
        "module",                  // 任务名
        SCHED_DEFAULT,             // 调度策略
        SCHED_PRIORITY_DEFAULT,    // 优先级
        1024,                      // 栈大小（字节）
        (px4_main_t)&run_trampoline,
        (char *const *)argv
    );
    // ...
}
```

---

## SITL 验证命令

```sh
# 启动示例模块
work_item_example start

# 查看运行状态（会调用 print_status()）
work_item_example status

# 停止模块
work_item_example stop

# 查看模块帮助
work_item_example help
```

---

## 今日检查清单

- [ ] 阅读 `px4_simple_app.c`，能解释为什么它不需要 `ScheduledWorkItem`
- [ ] 阅读 `WorkItemExample.hpp`，找到 3 个父类并能解释各自作用
- [ ] 阅读 `WorkItemExample.cpp`，标注 `init()` 和 `Run()` 的触发条件
- [ ] 阅读 `template_module.cpp`，找到 `task_spawn` 并理解栈大小参数
- [ ] 在 SITL 中执行 `work_item_example start` 和 `status`，确认模块运行

---

## 可验证产物

在代码文件中用注释标注关键流程节点（或在笔记中写出完整生命周期说明）。

---

## 扩展思考（有余力时）

- `ScheduledWorkItem` 与普通线程的区别：Work Queue 线程复用，不独占 CPU
- `ModuleParams::updateParams()` 的作用：批量刷新所有 `DEFINE_PARAMETERS` 中声明的参数缓存
- `should_exit()` 信号来自哪里？—— `ModuleBase::request_stop()` 设置退出标志
