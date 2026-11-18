# EasyX_homework

基于 EasyX + hiEasyX 的卡牌肉鸽（类杀戮尖塔）课程作业。逻辑层纯 C++（`src/hpps/`），
渲染层为 `src/game.cpp`（EasyX GUI）；两者仅通过回调接口通信。
本项目引入了基于 EasyX 的开源库 HiEasyX

## 目录结构

```
EasyX_homework/
├─ README.md                # 本文件
├─ docs/                    # CHANGELOG.md（决策记录）、TODO.md
├─ src/                     # 源代码
│  ├─ game.cpp              # GUI 入口（EasyX + hiEasyX 渲染层）
│  ├─ cli_test.cpp          # CLI 入口
│  └─ hpps/                 # 逻辑层头文件（core / battle / run / events / ...）
├─ res/                     # 资源
│  ├─ resource.rc           # 内嵌资源清单（RCDATA）
│  ├─ resource.h            # 资源 ID 定义
│  └─ assets/               # 图片 / 字体素材（bg / enemies / player / fonts）
├─ scripts/
│  └─ build-resource.ps1    # res/resource.rc → build/resource.o（GBK 转码 + windres）
├─ libs/HieasyX/hi-easyx-main/  # hiEasyX 库（含本项目的字体适配补丁）
├─ build/                   # 中间产物（resource.o 等，gitignore）
└─ bin/                     # 可执行文件输出（gitignore）
```

## 构建（VS Code）

1. 打开要编译的源文件（通常是 `src/game.cpp`），按 `Ctrl+Shift+B` 运行默认构建任务
   `build-easyx-mingw64`。任务链为：
   - `build-resource`：运行 `scripts/build-resource.ps1`，
     把 `res/resource.rc`（UTF-8）转码为 GBK 后用 windres 编译为 `build/resource.o`，
     同时确保 `build/`、`bin/` 目录存在；
   - `build-easyx-mingw64`：MSYS2 MinGW64 g++（C++23）编译当前文件，
     链接 `build/resource.o` 与 hiEasyX 全部源码（`sources.rsp`），输出到 `bin/`。
2. 图片与字体通过 RCDATA 内嵌进 exe，运行时**不依赖** `bin/assets/` 等外部文件；
   缺资源时代码自动回退占位色块 / 系统字体。

## 运行

```
bin\game.exe [种子短码]
```

- 命令行带种子短码（如 `8T7Wy0VBdW7`）可复现同一局；不带则随机。
- `src/cli_test.cpp` 编译出的 `bin/cli_test.exe` 为命令行游戏程序。

## 约定

- 逻辑层（`src/hpps/`）禁止引入图形库；GUI 不消耗任何逻辑随机数。
- 新增/替换素材：把文件放进 `res/assets/` 对应子目录，
  在 `res/resource.h` + `res/resource.rc` 登记后重新构建即可。

## AI-Assisted Development

AI was used for:
- Technical research and architectural discussions.
- Code generation and implementation suggestions.
- Debugging.
- Documentation.
- Asset generation.

AI Tools: TraeCode (DeepSeek v4.1 flash), ChatGPT
