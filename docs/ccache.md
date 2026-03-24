# 编译器缓存使用指南

仓颉编译器支持使用编译器缓存工具（ccache 或 sccache）来加速编译过程。本文档介绍如何配置和使用这些工具。

## 支持的缓存工具

| 工具 | 说明 |
|------|------|
| **ccache** | 经典的编译器缓存工具，本地缓存，配置简单 |
| **sccache** | Mozilla 开发的编译器缓存工具，支持分布式缓存（S3、Redis、GCS 等） |

## 配置方式

### CMake 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CANGJIE_ENABLE_CCACHE` | 是否启用编译器缓存 | `ON` |
| `CANGJIE_CACHE_TOOL` | 指定缓存工具：`auto`、`ccache`、`sccache` | `auto` |

### 使用示例

```bash
# 自动检测（优先 ccache，其次 sccache）
cmake -B build -DCANGJIE_ENABLE_CCACHE=ON

# 明确指定使用 sccache
cmake -B build -DCANGJIE_ENABLE_CCACHE=ON -DCANGJIE_CACHE_TOOL=sccache

# 明确指定使用 ccache
cmake -B build -DCANGJIE_ENABLE_CCACHE=ON -DCANGJIE_CACHE_TOOL=ccache

# 禁用缓存
cmake -B build -DCANGJIE_ENABLE_CCACHE=OFF
```

## 安装缓存工具

### ccache

**Linux (Debian/Ubuntu):**
```bash
sudo apt install ccache
```

**Linux (RHEL/CentOS/Fedora):**
```bash
sudo dnf install ccache
# 或
sudo yum install ccache
```

**macOS:**
```bash
brew install ccache
```

### sccache

**预编译二进制（推荐）:**
```bash
# 从 GitHub Releases 下载
# https://github.com/mozilla/sccache/releases
```

**Cargo 安装:**
```bash
cargo install sccache
```

**Linux (Debian/Ubuntu):**
```bash
sudo apt install sccache
```

## ccache 配置

ccache 默认缓存目录为 `~/.cache/ccache`，可通过环境变量或配置文件调整：

```bash
# 设置最大缓存大小（默认 5GB）
ccache --max-size=10G

# 查看缓存统计
ccache --show-stats

# 清空缓存
ccache --clear
```

配置文件 `~/.ccache/ccache.conf` 示例：
```
max_size = 10G
cache_dir = ~/.cache/ccache
```

## sccache 配置

sccache 支持多种存储后端，配置文件位于 `~/.config/sccache/config` 或由环境变量 `SCCACHE_CONF` 指定。

### 本地磁盘缓存

```json
{
  "cache": {
    "directory": "~/.cache/sccache"
  }
}
```

### Redis 缓存

```json
{
  "cache": {
    "redis": {
      "url": "redis://localhost:6379"
    }
  }
}
```

### S3 缓存

```json
{
  "cache": {
    "s3": {
      "bucket": "my-sccache-bucket",
      "region": "us-east-1"
    }
  }
}
```

### 常用命令

```bash
# 查看缓存统计
sccache --show-stats

# 启动服务器
sccache --start-server

# 停止服务器
sccache --stop-server

# 清空缓存
sccache --zero-stats
```

## 工作原理

CMake 通过以下方式启用编译器缓存：

1. **RULE_LAUNCH_COMPILE / RULE_LAUNCH_LINK**: 全局属性，在编译/链接命令前注入缓存工具
2. **CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER**: 编译器启动器变量，适用于 Ninja 生成器

设置后，编译命令会自动变为：
```bash
# 原始命令
gcc -c source.c -o source.o

# 启用缓存后
ccache gcc -c source.c -o source.o
```

## 注意事项

1. **Ninja 生成器**: 如果使用 Ninja 作为构建系统，需要设置 `CMAKE_*_COMPILER_LAUNCHER` 变量（本项目已自动处理）

2. **链接缓存**: `RULE_LAUNCH_LINK` 对链接步骤的缓存效果有限，主要加速的是编译步骤

3. **首次编译**: 首次编译时缓存为空，不会有加速效果；后续编译会显著加速

4. **缓存失效**: 以下情况会导致缓存失效：
   - 编译器版本变化
   - 编译选项变化
   - 源文件修改

5. **CI/CD 环境**: 建议在 CI 中使用 sccache 配合分布式存储，实现跨构建的缓存共享
