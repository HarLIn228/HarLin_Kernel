# HCC 编译器使用说明

HCC 是 HarLin C Compiler 的缩写，用于把 C 源文件编译成 HarLin 可执行格式 `.chc`。

## 命令格式

```
hcc [选项] <源文件.c>
```


## 常用选项

| 选项 | 说明 |
|------|------|
| `-o <文件>` | 指定输出文件，默认生成同名 `.chc` 文件 |
| `-e <符号>` | 指定入口函数，默认是 `_start` |
| `-h` | 显示帮助信息 |

## 示例

编译单个源文件：

```
hcc tests/hello.c -o tests/hello.chc
```

指定入口函数：

```
hcc app/main.c -o app/main.chc -e kernel_main
```

## 程序要求

- 源文件必须包含 HarLin 头文件 `#include "harlin.h"`
- 入口函数默认名为 `_start`
- 使用 `harlin_print` 输出字符串
- 使用 `harlin_exit` 结束程序

## 最小示例

```c
#include "harlin.h"

void _start(void)
{
    harlin_print("Hello World\n");
    harlin_exit(0);
}
```

## 构建 HCC 工具

在 `hcc` 目录下运行：

```
build_hcc.bat
```

编译成功后，`bin/HCC.exe` 和 `bin/bin2h.exe` 会被生成到根目录的 `bin` 文件夹中。

## 头文件路径

HCC 默认从 `hcc/include` 目录查找 `harlin.h`，因此建议在项目根目录执行 HCC。
