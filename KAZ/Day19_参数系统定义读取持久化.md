# Day 19：参数系统学习（定义、读取、持久化）

## 目标
理解参数如何定义、如何在代码中读取、如何持久化存储，掌握 param 相关 shell 命令。

---

## 第一步：参数定义语法

PX4 有两种定义参数的方式：

### 方式 1：C 语言宏定义（传统写法）

**参考文件：** `src/modules/commander/commander_params.c`

```c
/**
 * Roll trim
 * @group Radio Calibration
 * @min -0.25
 * @max 0.25
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(TRIM_ROLL, 0.0f);

/**
 * System autostart ID
 * @group System
 * @min 0
 * @max 99999
 */
PARAM_DEFINE_INT32(SYS_AUTOSTART, 0);
```

关键宏：
| 宏 | 类型 |
|---|---|
| `PARAM_DEFINE_INT32(name, default)` | 32 位整数参数 |
| `PARAM_DEFINE_FLOAT(name, default)` | 单精度浮点参数 |

### 方式 2：C++ DEFINE_PARAMETERS 宏（现代写法）

**参考文件：** `src/examples/work_item/WorkItemExample.hpp`

```cpp
// 在类定义中声明：
DEFINE_PARAMETERS(
    (ParamInt<px4::params::SYS_AUTOSTART>) _param_sys_autostart,
    (ParamFloat<px4::params::COM_RC_IN_MODE>) _param_rc_mode
)

// 在 Run() 中使用：
int val = _param_sys_autostart.get();
```

优点：
- 参数值自动缓存，无需每次调用 `param_get()`
- 调用 `updateParams()` 后自动更新所有缓存值

---

## 第二步：参数读取 API

**文件：** `src/lib/parameters/param.h`

### 底层 C API

```c
// 1. 通过名称查找参数句柄（启动时调用一次）
param_t handle = param_find("SYS_AUTOSTART");

// 2. 读取参数值
int32_t val;
param_get(handle, &val);

// 3. 修改参数值
int32_t new_val = 10017;
param_set(handle, &new_val);

// 4. 保存到持久化文件
param_save_default();

// 5. 重置为默认值
param_reset(handle);
```

### 参数类型检查

```c
param_type_t type = param_type(handle);
// PARAM_TYPE_INT32 = 1
// PARAM_TYPE_FLOAT = 2
```

---

## 第三步：持久化存储机制

**参数存储文件：**
| 文件 | 说明 |
|---|---|
| `parameters.bson`（工程根目录） | SITL 主参数文件，启动时 `param import` 加载 |
| `parameters_backup.bson`（工程根目录） | 备份文件，主文件损坏时自动回退 |

**持久化流程（rcS 第 61–100 行）：**

```sh
param select parameters.bson           # 指定主参数文件
param import                           # 从文件加载到内存
# 运行中 param set 修改的是内存中的值
param save                             # 将内存中的值写回文件
param select-backup parameters_backup.bson
```

**实现文件：** `src/lib/parameters/parameters.cpp`
- `param_save_default()`：序列化所有参数为 BSON 格式写入文件
- `param_import()`：从 BSON 文件反序列化恢复参数

---

## 参数系统完整路径

```
参数定义
  PARAM_DEFINE_FLOAT(MY_PARAM, 1.0f)      ← commander_params.c 等文件
        │
        ▼ 构建时由 px_process_params.py 解析
  生成元数据（Tools/px_process_params.py）
        │
        ▼
  系统启动时
  param import parameters.bson             ← rcS 加载持久化值
        │
        ▼
  代码中读取
  param_find("MY_PARAM") → handle
  param_get(handle, &val)                  ← 读取内存中的当前值
        │
        ▼
  修改并持久化
  param_set(handle, &new_val)              ← 修改内存值
  param_save()                             ← 写入 parameters.bson
```

---

## SITL 验证命令

```sh
# 查看单个参数
param show COM_RC_IN_MODE

# 查看包含关键字的所有参数
param show -c SYS_

# 修改参数（运行时生效）
param set COM_RC_IN_MODE 1

# 手动持久化（写入 parameters.bson）
param save

# 查看参数系统状态（加载/保存次数等）
param status

# 重置所有参数为默认值（危险！）
param reset_all

# 导出所有参数到指定文件
param export /tmp/my_params.bson
```

---

## 今日检查清单

- [ ] 阅读 `src/modules/commander/commander_params.c` 前 100 行，理解 `@group`、`@min`、`@max` 注解含义
- [ ] 阅读 `src/lib/parameters/param.h`，找到 `param_find()`、`param_get()`、`param_set()` 函数签名
- [ ] 在 SITL 中执行 `param show COM_RC_IN_MODE`，修改后执行 `param save`，重启后验证值是否保留
- [ ] 查看 `parameters.bson` 文件大小（`ls -lh parameters.bson`），确认持久化文件存在

---

## 可验证产物

用文字写出"一个参数从 .c 文件定义到运行时被读取再到持久化"的完整路径（3–5 句话）。

---

## 扩展思考（有余力时）

- `param_find()` 是线性查找，对性能敏感的模块应在初始化时调用一次并缓存 handle
- `DEFINE_PARAMETERS` 宏在背后自动做了 `param_find` 并缓存，`updateParams()` 批量刷新
- BSON 是 Binary JSON 格式，`bsondump` 命令可以将 `.bson` 文件以人类可读格式打印
