# Day 20：实操——新增测试参数并在模块中读取打印

## 目标
完成一次完整的"定义参数 → 代码读取 → 编译 → 仿真验证"闭环，建立修改模块的肌肉记忆。

---

## 操作路径总览

```
Step 1  在 WorkItemExample.hpp 中声明新参数
        ↓
Step 2  在 WorkItemExample.cpp 的 Run() 中读取并打印
        ↓
Step 3  确认 CMakeLists.txt 已启用该模块
        ↓
Step 4  重新编译
        ↓
Step 5  启动 SITL，执行模块，观察打印输出
```

---

## Step 1：在 WorkItemExample.hpp 中声明新参数

**文件：** `src/examples/work_item/WorkItemExample.hpp`

找到 `DEFINE_PARAMETERS` 块（在类定义末尾），新增一个参数。
以读取已有参数 `COM_RC_IN_MODE` 为例（不需要新建参数文件，直接复用）：

```cpp
// 修改前（原有参数声明区域，找到类似结构）：
DEFINE_PARAMETERS(
    (ParamFloat<px4::params::MPC_XY_P>) _param_mpc_xy_p   // 已有示例
)

// 修改后（新增一行）：
DEFINE_PARAMETERS(
    (ParamFloat<px4::params::MPC_XY_P>) _param_mpc_xy_p,
    (ParamInt<px4::params::COM_RC_IN_MODE>) _param_rc_in_mode   // 新增
)
```

> **注意：** 若原文件中 `DEFINE_PARAMETERS` 内容不同，按实际内容找到末尾添加，不要删除已有行。

---

## Step 2：在 Run() 中读取并打印

**文件：** `src/examples/work_item/WorkItemExample.cpp`

在 `Run()` 函数中参数更新检测之后，加入打印逻辑：

```cpp
void WorkItemExample::Run()
{
    if (should_exit()) { ... }

    // 检查参数更新（已有代码）
    if (_parameter_update_sub.updated()) {
        parameter_update_s param_update;
        _parameter_update_sub.copy(&param_update);
        updateParams();
    }

    // ---- 新增：每次 Run 打印参数值 ----
    PX4_INFO("COM_RC_IN_MODE = %d", (int)_param_rc_in_mode.get());
    // ---- 新增结束 ----

    // 原有 sensor_accel 读取逻辑 ...
}
```

> **注意：** `Run()` 会以传感器数据频率（~100–400 Hz）触发，实际使用中应加频率限制（如每 1 秒打印一次），避免刷屏。加上条件：
> ```cpp
> static hrt_abstime last_print = 0;
> if (hrt_elapsed_time(&last_print) > 1_s) {
>     last_print = hrt_absolute_time();
>     PX4_INFO("COM_RC_IN_MODE = %d", (int)_param_rc_in_mode.get());
> }
> ```

---

## Step 3：确认 CMakeLists.txt 已启用模块

**文件：** `src/examples/work_item/CMakeLists.txt`

确认构建目标存在（通常已有，不需修改）：

```cmake
px4_add_module(
    MODULE examples__work_item
    MAIN work_item_example
    SRCS
        WorkItemExample.cpp
    DEPENDS
        ...
)
```

若模块没有被默认构建目标包含，查看 `boards/px4/sitl/default.px4board`，
在 `CONFIG_EXAMPLES_WORK_ITEM` 行确认值为 `y`：

```
# 查找命令
grep -r "work_item" boards/px4/sitl/
```

---

## Step 4：重新编译

```sh
cd /home/wrj/px4/source-code/PX4-Autopilot

# 增量编译（只重编修改的文件，速度快）
make px4_sitl_default
```

**常见编译错误排查：**

| 错误信息 | 可能原因 |
|---|---|
| `error: 'COM_RC_IN_MODE' is not a member of 'px4::params'` | 参数名拼写错误，去 `src/modules/commander/commander_params.c` 确认正确名称 |
| `error: expected ',' or ')'` | `DEFINE_PARAMETERS` 最后一个参数后面多了逗号 |
| `undefined reference to ...` | CMakeLists.txt 缺少依赖 |

---

## Step 5：启动 SITL 并验证

```sh
# 启动仿真（iris 机型）
make px4_sitl_default none_iris

# 在 PX4 shell 中执行：
work_item_example start

# 观察输出，应看到类似：
# INFO  [work_item_example] COM_RC_IN_MODE = 1

# 修改参数后验证是否实时更新：
param set COM_RC_IN_MODE 0
# 等待约 1 秒，观察打印值是否变为 0

# 停止模块
work_item_example stop
```

---

## 今日检查清单

- [ ] 成功修改 `WorkItemExample.hpp`，新增参数声明
- [ ] 成功修改 `WorkItemExample.cpp`，在 `Run()` 中打印参数值（含频率限制）
- [ ] 编译通过，无报错
- [ ] 在 SITL 中看到参数值被正确打印
- [ ] 通过 `param set` 修改参数后，模块打印值实时更新

---

## 可验证产物

终端中看到自己新增的参数值被正确打印的截图或日志记录。

---

## 扩展思考（有余力时）

- `hrt_elapsed_time()` 中的 `hrt` 即 High Resolution Timer，是 PX4 的高精度时钟 API
- `_param_rc_in_mode.get()` 只返回缓存值，只有 `updateParams()` 被调用后才会刷新
- 如果想新增一个全新参数（不复用已有参数），需要在某个 `_params.c` 文件中用 `PARAM_DEFINE_INT32` 定义，并在同目录的 `CMakeLists.txt` 中将该文件加入 `SRCS`
