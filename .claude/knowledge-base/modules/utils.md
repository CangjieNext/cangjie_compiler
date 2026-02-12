# Module: utils

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

通用工具函数和辅助模块。该模块提供编译器各个部分共用的基础工具函数和辅助功能，包括文件操作、Unicode 处理、信号处理、性能分析、类型转换等通用功能。

## Key Components

- **FileUtil**: 文件操作工具，提供文件读写、路径处理等功能
- **Unicode**: Unicode 处理工具，支持 Unicode 字符识别和规范化
- **Signal**: 信号处理，处理系统信号和异常
- **Profiling**: 性能分析工具，包括 ProfileRecorder 和 UserTimer
- **Casting**: 类型转换工具，提供安全的类型转换功能
- **SafePointer**: 安全指针，提供指针安全检查
- **Semaphore**: 信号量，支持并发控制
- **TaskQueue**: 任务队列，支持并行任务调度
- **ParallelUtil**: 并行工具，提供并行计算支持
- **StdUtils**: 标准工具函数集合
- **UnicodeTables**: Unicode 字符表数据

## Public APIs

- `FileUtil::readFile()` - 读取文件
- `FileUtil::writeFile()` - 写入文件
- `FileUtil::exists()` - 检查文件是否存在
- `Unicode::isIdentifier()` - 判断是否为标识符字符
- `Unicode::normalize()` - Unicode 规范化
- `Signal::registerHandler()` - 注册信号处理器
- `ProfileRecorder::start()` - 开始性能记录
- `ProfileRecorder::stop()` - 停止性能记录
- `ProfileRecorder::report()` - 生成性能报告
- `SafePointer::get()` - 安全获取指针
- `Semaphore::wait()` - 等待信号量
- `Semaphore::signal()` - 发送信号量
- `TaskQueue::enqueue()` - 添加任务
- `TaskQueue::execute()` - 执行任务

## Dependencies

无直接依赖，作为基础工具模块被其他所有模块使用。

## Source Locations

- **Implementation**: `src/Utils/`
  - 各种工具函数的实现
  - `StdUtils/` 子目录包含标准工具函数
  - `UnicodeTables/` 子目录包含 Unicode 字符表数据

- **Headers**: `include/cangjie/Utils/`
  - `Utils.h` - 通用工具函数
  - `FileUtil.h` - 文件操作工具
  - `Unicode.h` - Unicode 处理
  - `Signal.h` - 信号处理
  - `UserTimer.h` - 用户计时器
  - `ProfileRecorder.h` - 性能记录器
  - `FloatFormat.h` - 浮点格式化
  - `ICEUtil.h` - 编译时求值工具
  - `SafePointer.h` - 安全指针
  - `Semaphore.h` - 信号量
  - `Casting.h` - 类型转换
  - `CheckUtils.h` - 检查工具
  - `ConstantsUtils.h` - 常量工具
  - `Macros.h` - 宏定义
  - `ParallelUtil.h` - 并行工具
  - `TaskQueue.h` - 任务队列

- **Tests**: `unittests/Utils/`

## Usage Examples

```cpp
// 文件操作
std::string content = FileUtil::readFile("/path/to/file.cj");
FileUtil::writeFile("/path/to/output.txt", content);

// Unicode 处理
bool isIdent = Unicode::isIdentifier(ch);
std::string normalized = Unicode::normalize(str);

// 性能分析
ProfileRecorder profiler;
profiler.start("compilation");
// ... 执行编译任务 ...
profiler.stop("compilation");
profiler.report();

// 并行任务
TaskQueue queue;
queue.enqueue([]() { /* 任务 1 */ });
queue.enqueue([]() { /* 任务 2 */ });
queue.execute();
```

## Related Concepts

- 工具函数设计模式
- Unicode 标准
- 并发编程
- 性能优化

## Related Workflows

- 所有编译流程都依赖这些基础工具
- 性能分析和优化
- 并行编译支持
