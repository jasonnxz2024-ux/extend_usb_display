# Windows H.264 上位机改造

本目录的补丁**基于 chuanjinpang 的 ESP32-S2 USB 扩展屏 Windows IDD 项目改造**：

- 上游仓库：<https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display>
- 补丁基线提交：`c2de0f56f778904d663d8e2dc5a71b4a6b270ecc`
- 适用目录：上游仓库根目录

上游项目提供 Windows 间接显示驱动（IDD）、WinUSB 传输及 RGB/JPEG 编码。本地改造增加 Windows Media Foundation H.264 编码器、Annex B access unit 输出、`Eh2644` 产品字符串识别，并将 INF 的 USB ID 从上游 ESP32 示例值调整为本设备端的 `USB\VID_33C3&PID_0E05`。它是**基于 ESP32 项目的 Windows 上位机改造**，设备端仍是 ArtInChip d21x 上运行的 RT-Thread / Luban-Lite 应用。

## 获取与应用

在安装 Git 的 Windows 开发环境中，先获取上游指定提交，再从本仓库应用补丁：

```powershell
git clone https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display.git
cd win10_idd_xfz1986_usb_graphic_driver_display
git checkout c2de0f56f778904d663d8e2dc5a71b4a6b270ecc
git apply --check C:\path\to\extend_usb_display\windows-host\xfz1986-h264.patch
git apply C:\path\to\extend_usb_display\windows-host\xfz1986-h264.patch
```

打开 `win10_idd_xfz1986_usb_graphic_driver/idd_xfz1986_usb_graphic.sln`，构建 `Release|x64`。本地工程配置使用 Windows SDK `10.0.28000.0` 和 `Microsoft.Windows.WDK.x64` `10.0.28000.2526` NuGet 包，需在 Windows/Visual Studio 中安装或还原相应构建环境。驱动安装还需按 Windows 要求签名；本仓库不提供已签名驱动或安装程序。

补丁没有复制上游完整项目：`xfz1986-h264.patch` 只包含本地 H.264 改造所需的源码、工程配置和 INF 差异。上游仓库根目录未见明确许可证文件，因此本仓库没有为其代码另行声明许可证；分发衍生驱动及二进制前应核对上游项目的许可授权。

## 与设备端对应关系

| 项目 | 值 |
| --- | --- |
| USB VID:PID | `33C3:0E05` |
| Windows 设备字符串 | `aicudisp0_R1024x600_Eh2644_Fps30_Bl1024` |
| 帧头 | 16 字节，沿用 xfz1986 v2 协议 |
| 编码类型 | `4`（H.264） |
| 帧数据 | 完整 Annex B access unit，最大 1 MiB |
| 接收端点 | Bulk OUT `0x02` |

设备端协议定义见 `application/rt-thread/usb_h264_display/aic_usb_display_protocol.h`。本地验证仅覆盖补丁对指定上游提交的应用，尚无本次打包后的 Windows 编译和设备联调结果。
