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
