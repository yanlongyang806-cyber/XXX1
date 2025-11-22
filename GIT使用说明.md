# Git 仓库使用说明

## 当前状态

Git仓库已经初始化完成，包含以下内容：

- ✅ 项目所有源代码文件
- ✅ `.gitignore` 文件（排除编译输出文件）
- ✅ `README.md` 项目说明文档
- ✅ `编译说明.md` 编译指南
- ✅ 初始提交已完成

## 常用Git命令

### 查看状态
```bash
git status
```

### 查看提交历史
```bash
git log
```

### 添加文件到暂存区
```bash
git add <文件名>
# 或添加所有文件
git add .
```

### 提交更改
```bash
git commit -m "提交说明"
```

### 查看差异
```bash
git diff
```

## 连接远程仓库

### 1. 在Git托管平台创建仓库

如果您还没有远程仓库，可以在以下平台创建：
- GitHub (https://github.com)
- GitLab (https://gitlab.com)
- Gitee (https://gitee.com) - 中国用户推荐
- 自建Git服务器

### 2. 添加远程仓库

创建远程仓库后，执行以下命令：

```bash
# 添加远程仓库（替换为您的仓库URL）
git remote add origin <远程仓库URL>

# 例如：
# git remote add origin https://github.com/用户名/仓库名.git
# 或
# git remote add origin git@github.com:用户名/仓库名.git
```

### 3. 推送到远程仓库

```bash
# 首次推送
git push -u origin main

# 或者如果默认分支是master
git push -u origin master

# 后续推送
git push
```

## 分支管理

### 创建新分支
```bash
git branch <分支名>
git checkout <分支名>
# 或使用新语法
git checkout -b <分支名>
```

### 查看分支
```bash
git branch
```

### 切换分支
```bash
git checkout <分支名>
```

### 合并分支
```bash
git merge <分支名>
```

## 忽略文件说明

`.gitignore` 文件已配置为忽略以下内容：
- Visual Studio 编译输出文件（.exe, .dll, .obj等）
- 临时文件和缓存
- 自动生成的文件
- 调试文件

这些文件不需要提交到仓库，因为可以通过编译重新生成。

## 提交规范

建议使用清晰的提交信息：
- `feat: 添加新功能`
- `fix: 修复bug`
- `docs: 更新文档`
- `refactor: 代码重构`
- `style: 代码格式调整`

例如：
```bash
git commit -m "feat: 实现全地图强制PVP功能"
```

## 注意事项

1. **不要在仓库中提交敏感信息**（如密码、API密钥等）
2. **编译输出文件已自动忽略**，无需手动排除
3. **定期提交代码**，保持提交历史清晰
4. **推送前先拉取**，避免冲突：
   ```bash
   git pull origin main
   git push origin main
   ```

## 下一步操作

1. 如果您已有远程仓库，执行：
   ```bash
   git remote add origin <您的仓库URL>
   git push -u origin main
   ```

2. 如果还没有远程仓库：
   - 在Git托管平台创建新仓库
   - 按照上面的步骤添加远程仓库并推送

3. 团队成员克隆仓库：
   ```bash
   git clone <远程仓库URL>
   ```

