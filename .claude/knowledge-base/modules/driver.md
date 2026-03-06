# Module: driver

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

编译驱动，协调整个编译流程。该模块是编译器的顶层控制模块，负责协调各个编译阶段的执行，管理编译选项，调度编译任务，以及与后端工具链的交互。

## Key Components

- **Driver**: 主驱动器，编译流程的核心控制器，协调整个编译过程
- **Tool**: 编译工具抽象，封装各种编译工具（编译器、链接器、汇编器等）
- **ToolOptions**: 工具选项管理，处理命令行参数和编译选项
- **TempFileManager**: 临时文件管理器，管理编译过程中的临时文件
- **Job**: 编译任务，表示编译流程中的单个任务单元
- **Backend**: 后端工具支持，包含各种后端工具的实现
- **Toolchains**: 工具链支持，管理不同平台的工具链配置

## Public APIs

- `Driver::buildCompilation()` - 构建编译任务
- `Driver::executeJobs()` - 执行编译任务
- `Tool::constructCommand()` - 构造工具命令
- `Tool::execute()` - 执行工具
- `ToolOptions::parse()` - 解析工具选项
- `TempFileManager::createTempFile()` - 创建临时文件
- `TempFileManager::cleanup()` - 清理临时文件
- `Job::execute()` - 执行任务

## Dependencies

- **codegen**: 代码生成模块，生成目标代码
- **frontend**: 前端模块，提供词法、语法、语义分析

## Source Locations

- **Implementation**: `src/Driver/`
  - 主驱动器实现
  - `Backend/` 子目录包含后端工具支持
  - `Toolchains/` 子目录包含工具链配置

- **Headers**: `include/cangjie/Driver/`
  - `Driver.h` - 主驱动器接口
  - `Tool.h` - 工具抽象接口
  - `ToolOptions.h` - 工具选项接口
  - `DriverOptions.h` - 驱动器选项
  - `TempFileManager.h` - 临时文件管理器接口
  - 其他驱动相关头文件

- **Tests**: `unittests/Driver/`

## Usage Examples

```cpp
// 创建驱动器实例
Driver driver;

// 设置编译选项
DriverOptions options;
options.setInputFiles({"main.cj"});
options.setOutputFile("main.o");
options.setOptimizationLevel(2);

// 构建编译任务
Compilation compilation = driver.buildCompilation(options);

// 执行编译
int result = driver.executeJobs(compilation);
```

命令行使用示例：
```bash
cjc main.cj -o main.o -O2
```

## Related Concepts

- 编译流程管理
- 任务调度
- 工具链配置

## Related Workflows

- [Building](../workflows/building.md)
