# GitHub Actions 自动编译说明

## 已配置的CI/CD

已创建 GitHub Actions 工作流文件，实现自动编译功能。

### 工作流文件位置
- `.github/workflows/build.yml`

## 触发条件

自动编译会在以下情况触发：

1. **Push 到 main/master 分支** - 每次代码推送都会自动编译
2. **Pull Request** - 每次PR都会编译验证
3. **手动触发** - 可以在GitHub Actions页面手动运行

## 编译配置

- **操作系统**: Windows Latest
- **编译器**: MSBuild (Visual Studio 2019/2022)
- **配置**: Debug
- **平台**: Win32

## 查看编译结果

1. 访问您的GitHub仓库：https://github.com/yanlongyang806-cyber/XXX1
2. 点击 **Actions** 标签页
3. 选择最新的工作流运行
4. 查看编译日志和结果

## 下载编译产物

编译成功后，可以在Actions页面下载：

1. 进入工作流运行页面
2. 在 **Artifacts** 区域找到 `GameServer-Build-Artifacts`
3. 下载编译好的 `GameServer.exe` 文件

## 编译日志

- 编译过程中的所有日志都会保存在Actions页面
- 如果编译失败，可以在日志中查看错误信息

## 手动触发编译

如果您想立即触发一次编译：

1. 访问：https://github.com/yanlongyang806-cyber/XXX1/actions
2. 选择 **Build GameServer** 工作流
3. 点击 **Run workflow** 按钮
4. 选择分支（main）并点击 **Run workflow**

## 注意事项

1. **首次编译可能需要较长时间** - 需要下载依赖和设置环境
2. **编译产物保留7天** - 之后会自动删除
3. **依赖项** - 如果项目有外部依赖，可能需要额外配置

## 编译状态徽章（可选）

您可以在README.md中添加编译状态徽章：

```markdown
![Build Status](https://github.com/yanlongyang806-cyber/XXX1/workflows/Build%20GameServer/badge.svg)
```

## 故障排除

如果编译失败：

1. 查看Actions页面的详细日志
2. 检查是否有依赖项缺失
3. 确认项目配置是否正确
4. 查看错误信息并根据提示修复

