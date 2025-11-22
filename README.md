# GameServer 项目

这是一个游戏服务器项目，支持全地图强制PVP功能。

## 项目结构

```
.
├── libs/                    # 库文件
│   ├── AILib/              # AI库
│   ├── ServerLib/          # 服务器库
│   ├── WorldLib/           # 世界库
│   └── ...
├── Night/                  # 游戏相关代码
│   ├── GameServer/         # 游戏服务器
│   ├── GameClient/         # 游戏客户端
│   └── Common/             # 通用代码
└── 编译说明.md            # 编译说明文档
```

## 功能特性

- **全地图强制PVP**：所有地图区域都被视为PVP区域，玩家可以在任何地图上互相攻击

## 编译说明

详细的编译步骤请参考 [编译说明.md](编译说明.md)

### 快速编译

使用 Visual Studio:
1. 打开 `Night/GameServer/NNOGameServer.sln`
2. 选择配置 (Debug/Full Debug)
3. 按 Ctrl+Shift+B 编译

使用命令行:
```cmd
cd Night\GameServer
msbuild NNOGameServer.sln /p:Configuration=Debug /p:Platform=Win32
```

## 修改说明

### PVP功能实现

在 `Night/GameServer/NNOFixup.c` 中实现了全地图强制PVP功能：

- 覆盖了 `worldRegionGetType` 函数，使所有区域类型都返回 `WRT_PvP`
- 这样所有地图都会被系统识别为PVP区域

## 系统要求

- Windows 操作系统
- Visual Studio 2010 或更高版本
- MSBuild 工具

## 许可证

请查看项目根目录下的许可证文件。

