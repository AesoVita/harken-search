# Harken Search Windows 打包教程

这份教程的目标是：**别人第一次拿到源码，就能在 Windows 上直接编译并打包出可运行版本。**

适用仓库：

- Harken Search public repository checkout root

---

## 1. 环境要求

建议使用：

- Windows 10 / 11 x64
- [Visual Studio 2022](https://visualstudio.microsoft.com/)
  - 安装工作负载：**Desktop development with C++**
- CMake 3.16+
- Git

> 说明：本仓库当前是 **Windows 优先构建**。

---

## 2. 获取源码

如果还没 clone：

```powershell
git clone https://github.com/xindong/harken-search.git
cd harken-search
```

如果你拿到的是压缩包源码，直接解压后进入仓库根目录即可。

文中提到的路径约定：

- **repo root**：源码检出目录
- **build output**：`build_release/Release` 或 `build_public/Release`

---

## 3. Internal/default build（`build_release`）

在仓库根目录执行：

```powershell
cmake -S . -B build_release -G "Visual Studio 17 2022" -A x64
cmake --build build_release --config Release --target HarkenSearch -- /m
```

internal/default 构建主程序在：

```text
build_release/Release/HarkenSearch.exe
```

---

## 4. Public release-safe build（`build_public`）

如需生成 **公开发布安全构建**，执行：

```powershell
cmake -S . -B build_public -G "Visual Studio 17 2022" -A x64 -DARIS_PUBLIC_RELEASE=ON -DAURO_BUNDLE_AVID_PTSL=OFF
cmake --build build_public --config Release --target HarkenSearch -- /m
```

公开发布安全构建主程序在：

```text
build_public/Release/HarkenSearch.exe
```


---

## 5. Public release preflight（提交 / 打包前必跑）

固定执行顺序：

```powershell
py tools/check_release_compliance.py
cmake -S . -B build_public -G "Visual Studio 17 2022" -A x64 -DARIS_PUBLIC_RELEASE=ON -DAURO_BUNDLE_AVID_PTSL=OFF
cmake --build build_public --config Release --target HarkenSearch -- /m
py tools/check_release_compliance.py --release-dir build_public/Release
```

`--release-dir` 这一步会额外检查：

- 必需 metadata 是否随包存在：`LICENSE`、`THIRD_PARTY_NOTICES.md`、`ASSET_PROVENANCE.md`、`BINARY_PROVENANCE.md`、`LICENSES/`、`PRIVACY.md`、`AI_FEATURE_DISCLOSURE.md`
- `BINARY_PROVENANCE.md` 中记录的 `WebView2Loader.dll` / `rlottie.dll` / `rlottie-image-loader.dll` 哈希是否与实际产物一致

---

## 6. 打包时应该拿哪些文件

**internal/default 构建最简单的打包方式：直接把整个 `build_release/Release/` 文件夹打包。公开发布安全构建则应打包整个 `build_public/Release/` 文件夹。**

当前公开发布目录至少应包含这些关键文件：

- `HarkenSearch.exe`
- `WebView2Loader.dll`
- `rlottie.dll`
- `rlottie-image-loader.dll`
- `lucide.ttf`
- `assets/`
- `models/`
- `splash_beta.png`
- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `ASSET_PROVENANCE.md`
- `BINARY_PROVENANCE.md`
- `LICENSES/`
- `PRIVACY.md`
- `AI_FEATURE_DISCLOSURE.md`

默认公开发布构建下，`build_public/Release/` **不应出现**：

- `PTSLC_CPP.dll`

**结论：不要只拷一个 exe。**

建议打包方式：

1. 编译完成后进入对应的 `Release` 目录
2. 全选所有文件和文件夹
3. 压缩为 zip
4. 把 zip 发给测试或同事

---

## 7. 推荐给同事的最短流程

### internal/default

```powershell
cmake -S . -B build_release -G "Visual Studio 17 2022" -A x64
cmake --build build_release --config Release --target HarkenSearch -- /m
```

打包目录：

```text
build_release/Release/
```

### public release-safe

```powershell
py tools/check_release_compliance.py
cmake -S . -B build_public -G "Visual Studio 17 2022" -A x64 -DARIS_PUBLIC_RELEASE=ON -DAURO_BUNDLE_AVID_PTSL=OFF
cmake --build build_public --config Release --target HarkenSearch -- /m
py tools/check_release_compliance.py --release-dir build_public/Release
```

打包目录：

```text
build_public/Release/
```

---

## 8. 可选 SDK 说明

仓库里有一些 **可选集成**，不是首次打包的必需条件：

### REAPER SDK（可选）

如需启用对应能力，可传入：

```powershell
-DAURO_REAPER_SDK_ROOT="你的 reaper-sdk\\sdk 路径"
```

### Avid PTSL / PTSLC_CPP（可选且默认不随包分发）

Harken Search 可以编译 `HarkenSearchProToolsBridge.exe`，但 **Avid 的 `PTSLC_CPP.dll` 默认不打进公开发布包**。

如需在本地接入你自己已获许可的 Avid SDK / runtime，可传入：

```powershell
-DAURO_PROTOOLS_PTSL_SDK_ROOT="你的本地 SDK 或 drop 路径"
```

只有在你已经完成手工许可确认、确实允许把 DLL 放进当前构建产物时，才额外传入：

```powershell
-DAURO_BUNDLE_AVID_PTSL=ON
```

否则保持默认：

```powershell
-DAURO_BUNDLE_AVID_PTSL=OFF
```

---

## 9. 当前仓库已经自带的依赖说明

当前仓库已经包含并接入了构建所需的主要第三方内容，不需要第一次就手动折腾：

- SQLite（源码内置）
- ImGui（源码内置）
- ixwebsocket（源码编译）
- whisper.cpp 相关库（仓库内已有构建产物）
- rlottie 相关运行库（仓库内已有构建产物）
- WebView2 SDK 文件（仓库内已有）

所以正常情况下，**装好 VS2022 + CMake 后就能直接编译。**

> 注意：仓库内随包复制的是 `WebView2Loader.dll`，它不是完整运行时；目标机器仍需安装 **Microsoft Edge WebView2 Runtime**。
