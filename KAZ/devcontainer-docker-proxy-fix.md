# Devcontainer 启动失败排查与修复说明

**日期**：2026-03-29
**环境**：WSL2 (Ubuntu 22.04) + Docker Engine + VS Code Dev Containers
**仓库**：PX4-Autopilot-personal-test

---

## 一、问题描述

在 VS Code 中使用 **Reopen in Container** 启动 devcontainer 时报错：

```
ERROR: failed to build: failed to solve: DeadlineExceeded:
px4io/px4-dev-nuttx-focal:2022-08-12: failed to resolve source metadata for
docker.io/px4io/px4-dev-nuttx-focal:2022-08-12: failed to do request:
Head "https://registry-1.docker.io/v2/px4io/px4-dev-nuttx-focal/manifests/2022-08-12":
dial tcp [2a03:2880:f111:83:face:b00c:0:25de]:443: i/o timeout
```

devcontainer 配置文件为 `.devcontainer/devcontainer.json`，所用基础镜像：

```
px4io/px4-dev-nuttx-focal:2022-08-12
```

---

## 二、根因分析

### 2.1 误判为镜像失效

第一步先用 `docker manifest inspect` 验证了镜像标签是否存在：

```bash
docker manifest inspect px4io/px4-dev-nuttx-focal:2022-08-12
```

返回了完整的 manifest JSON，**确认镜像仍然存在**，排除标签失效可能。

### 2.2 确认网络路径走 IPv6

错误信息中目标地址为 IPv6：

```
dial tcp [2a03:2880:f111:83:face:b00c:0:25de]:443: i/o timeout
```

说明 DNS 解析 `registry-1.docker.io` 时优先返回了 AAAA 记录，而该 IPv6 路径在本机网络环境下不通。

### 2.3 Docker daemon 未继承代理环境变量

检查当前 shell 环境变量：

```bash
echo $HTTP_PROXY   # http://127.0.0.1:29290
echo $HTTPS_PROXY  # http://127.0.0.1:29290
```

用户层 shell 设置了科学上网代理（端口 29290），**但 Docker daemon 以 root 运行，不继承用户 shell 的环境变量**，导致：

| 操作 | 是否走代理 | 结果 |
|------|-----------|------|
| `docker manifest inspect`（CLI 直连） | 是 | 成功 |
| `docker pull`（经过 daemon） | 否 | IPv6 超时 |
| devcontainer 构建（经过 daemon） | 否 | IPv6 超时 |

---

## 三、修复步骤

### 步骤 1：禁用 Docker daemon 的 IPv6 并指定 DNS（辅助）

创建 `/etc/docker/daemon.json`：

```json
{
  "ipv6": false,
  "dns": ["8.8.8.8", "1.1.1.1"]
}
```

> 注意：`"ipv6": false` 仅影响容器网络，不影响 daemon 自身出站连接，单独使用仍然超时。

### 步骤 2：固定 registry-1.docker.io 的 IPv4 地址（辅助）

先查询 IPv4 地址：

```bash
getent ahostsv4 registry-1.docker.io | awk '{print $1}' | sort -u
# => 31.13.112.9
```

写入 `/etc/hosts`：

```
31.13.112.9 registry-1.docker.io
```

> 注意：该条目绕过了 IPv6 解析，但 Docker daemon 本身没有代理时仍然无法访问 Docker Hub。

### 步骤 3：为 Docker daemon 配置代理（核心修复）

创建 systemd drop-in 文件，将用户层代理注入 Docker daemon：

```bash
sudo mkdir -p /etc/systemd/system/docker.service.d
sudo tee /etc/systemd/system/docker.service.d/http-proxy.conf > /dev/null << 'EOF'
[Service]
Environment="HTTP_PROXY=http://127.0.0.1:29290"
Environment="HTTPS_PROXY=http://127.0.0.1:29290"
Environment="NO_PROXY=localhost,127.0.0.1"
EOF
```

重新加载 systemd 并重启 Docker daemon：

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker
```

### 步骤 4：验证镜像拉取

```bash
sudo docker pull px4io/px4-dev-nuttx-focal:2022-08-12
```

输出：

```
2022-08-12: Pulling from px4io/px4-dev-nuttx-focal
...
Status: Downloaded newer image for px4io/px4-dev-nuttx-focal:2022-08-12
docker.io/px4io/px4-dev-nuttx-focal:2022-08-12
```

镜像拉取成功，devcontainer 可正常构建。

---

## 四、当前系统配置文件

### `/etc/systemd/system/docker.service.d/http-proxy.conf`

```ini
[Service]
Environment="HTTP_PROXY=http://127.0.0.1:29290"
Environment="HTTPS_PROXY=http://127.0.0.1:29290"
Environment="NO_PROXY=localhost,127.0.0.1"
```

### `/etc/docker/daemon.json`

```json
{
  "ipv6": false,
  "dns": ["8.8.8.8", "1.1.1.1"]
}
```

### `/etc/hosts`（新增条目）

```
31.13.112.9 registry-1.docker.io
```

---

## 五、重要注意事项

1. **代理端口变更**：若将来代理端口从 `29290` 变更，需同步更新 `/etc/systemd/system/docker.service.d/http-proxy.conf`，并重新执行 `sudo systemctl daemon-reload && sudo systemctl restart docker`。

2. **`/etc/hosts` 固定 IP 的风险**：Docker Hub 的 IP 地址可能发生变化，若将来拉取失败，可先删除该条目排查：
   ```bash
   sudo sed -i '/registry-1.docker.io/d' /etc/hosts
   ```

3. **代理工具重启后端口可能变化**：建议将代理工具设置为固定监听端口。

4. **devcontainer 重建**：镜像已缓存到本地，在 VS Code 中执行 **Rebuild and Reopen in Container** 时无需再次从远端拉取。

---

## 六、后续步骤

在 VS Code 命令面板（`Ctrl+Shift+P`）执行：

```
Dev Containers: Rebuild and Reopen in Container
```

devcontainer 将直接使用本地缓存镜像构建，不再需要访问 Docker Hub。

---

## 七、容器内应用无法使用宿主代理（Copilot / curl 等 ECONNREFUSED）

> **场景**：Docker daemon 已走代理可以拉取镜像，但容器启动后，容器内运行的应用（如 VS Code Copilot 扩展、curl、pip 等）试图通过 `127.0.0.1:29290` 访问代理时报 `ECONNREFUSED`。

### 7.1 问题描述

**现象**：

```
connect ECONNREFUSED 127.0.0.1:29290
```

WSL2 宿主机中可以正常连通代理（`nc -zv 127.0.0.1 29290` → `Connection succeeded`），但在 devcontainer 内执行同样命令则失败。

**环境变量已正确传入容器**：

```bash
# 容器内
echo $HTTP_PROXY   # http://127.0.0.1:29290  ← 已设置，但连不上
```

### 7.2 根因分析

#### 网络拓扑对比

```
Windows 主机
  └─ 代理进程 (Clash/V2Ray) 监听 0.0.0.0:29290

WSL2（mirrored 网络模式）
  └─ 127.0.0.1  ← 通过 WSL2 镜像网络映射到 Windows 的 loopback
                   因此 WSL 内 127.0.0.1:29290 可访问 Windows 代理 ?

Docker 容器（bridge 网络模式，默认）
  └─ 127.0.0.1  ← 容器自身的 loopback，与宿主完全隔离
                   访问 127.0.0.1:29290 → 容器内没有监听者 ?
```

| 位置 | `127.0.0.1` 指向 | `127.0.0.1:29290` 可达 |
|------|-----------------|------------------------|
| Windows | Windows loopback | ? 代理本身在此 |
| WSL2 宿主 | 通过 mirrored 模式映射到 Windows loopback | ? |
| Docker 容器（bridge） | 容器自身 loopback | ? |

曾尝试的宿主 IP 均失败：

```bash
# 在容器内测试（均失败）
nc -zv 172.17.0.1 29290   # Docker bridge 网关 ?
nc -zv 192.168.0.4 29290  # WSL2 的以太网 IP  ?
nc -zv 192.168.0.1 29290  # 路由器网关        ?
```

根本原因：Windows 代理绑定 `0.0.0.0:29290`，但 Windows 防火墙/路由配置下，这些 IP 路径对 Docker bridge 容器不通。

#### WSL2 mirrored 网络模式的特殊性

WSL2 开启 mirrored 模式（`.wslconfig` 中 `networkingMode=mirrored`）后：
- WSL2 内 `127.0.0.1` = Windows loopback（镜像关系）
- Docker 容器运行在 WSL2 内的 Docker Engine 上，但 **容器使用独立 bridge 网络**，不继承该镜像关系

### 7.3 解决方案：`--network=host`

让容器共享 WSL2 的网络栈，而非独立的 bridge 网络。

```
WSL2 网络栈（含 mirrored loopback → Windows 代理）
  └─ 容器（--network=host，共享同一网络命名空间）
       └─ 127.0.0.1:29290 → WSL2 mirrored → Windows 代理 ?
```

### 7.4 操作步骤

#### 步骤 1：修改 `devcontainer.json`

在 `.devcontainer/devcontainer.json` 的 `runArgs` 中添加 `--network=host`：

```jsonc
{
  "runArgs": [
    "--cap-add=SYS_PTRACE",
    "--security-opt", "seccomp=unconfined",
    "--network=host"          // ← 新增：共享 WSL2 网络栈
  ],
  // forwardPorts 与 --network=host 不兼容，须置空或删除
  "forwardPorts": []
}
```

> **注意**：`--network=host` 模式下容器与宿主共享所有端口，`forwardPorts` 配置无效且会报警告，需清空。

#### 步骤 2：清理旧容器

```bash
# 查看正在运行的 devcontainer（名称通常含 vsc- 前缀）
docker ps -a | grep vsc-

# 强制删除旧容器（替换 <container_id> 为实际 ID）
docker rm -f <container_id>
```

#### 步骤 3：重建容器

在 VS Code 命令面板（`Ctrl+Shift+P`）执行：

```
Dev Containers: Rebuild and Reopen in Container
```

#### 步骤 4：验证代理可达性

进入容器后，在终端执行：

```bash
# 方法 1：nc 测试 TCP 连通性
nc -zv 127.0.0.1 29290
# 期望输出：Connection to 127.0.0.1 29290 port [tcp/*] succeeded!

# 方法 2：curl 通过代理访问外网
curl -v --proxy http://127.0.0.1:29290 https://www.google.com -o /dev/null 2>&1 | head -5
# 期望：HTTP/1.1 200 或 301（非 connection refused）
```

### 7.5 注意事项

1. **端口冲突风险**：`--network=host` 让容器暴露所有端口到宿主，若容器内服务与宿主端口冲突需注意。

2. **`forwardPorts` 必须清空**：
   ```jsonc
   "forwardPorts": []   // 不能有具体端口，否则 VS Code 会报错/警告
   ```

3. **此方案仅适用于 Linux/WSL2 Docker**：macOS 和 Windows 原生 Docker Desktop 不支持 `--network=host`（会静默忽略）。

4. **代理工具须绑定 `0.0.0.0` 或 `127.0.0.1`**：本方案依赖 WSL2 的 mirrored 网络。若代理工具只监听 Windows 回环，WSL2 mirrored 模式下仍可路由；但若关闭 mirrored 模式（NAT 模式），则需改用其他方式。

5. **确认 WSL2 处于 mirrored 网络模式**：

   ```powershell
   # Windows PowerShell 中查看
   Get-Content "$env:USERPROFILE\.wslconfig"
   # 期望包含：
   # [wsl2]
   # networkingMode=mirrored
   ```

### 7.6 当前生效配置（本仓库）

**`.devcontainer/devcontainer.json`**（关键字段）：

```jsonc
{
  "name": "px4-dev-nuttx",
  "build": { "dockerfile": "Dockerfile", "context": ".." },
  "runArgs": [
    "--cap-add=SYS_PTRACE",
    "--security-opt", "seccomp=unconfined",
    "--network=host"
  ],
  "containerUser": "wrj",
  "containerEnv": { "LOCAL_USER_ID": "${localEnv:UID}" },
  "forwardPorts": []
}
```
