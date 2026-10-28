# USB H.264 扩展屏（RT-Thread / Luban-Lite）

这是从 Luban-Lite SDK 提取的独立**软件包**，包含 ArtInChip d21x 设备端的 USB H.264 扩展屏实现、`demo128-nand` 参考配置，以及基于 ESP32 USB 扩展屏 Windows IDD 项目的 H.264 改造补丁。发布到独立 Git 仓库时，可将本目录内容作为仓库根目录。它不包含完整 SDK、CherryUSB、MPP、Windows 驱动二进制或固件镜像。

## 工作方式与依赖

Windows 把桌面画面编码为 H.264，通过 WinUSB Bulk OUT 传给设备。设备端使用 CherryUSB 收帧、Luban-Lite MPP 硬件解码，并通过 framebuffer 视频层显示。设备端运行在 **RT-Thread** 上，使用 Luban-Lite 的 USB、VE、显示和 OSAL 接口；当前示例针对 **d21x / demo128-nand / 1024×600**，其他板卡需要移植 USB 端口、显示屏配置和内存配置。

包中包含以下文件：

```text
application/rt-thread/usb_h264_display/
  aic_usb_display_protocol.h  USB 帧头及协议常量
  usbd_winusb_display.c       WinUSB 接收、H.264 解码与显示
  main.c                      RT-Thread 应用入口
  Kconfig                     应用菜单
  SConscript                  SDK 内编译入口
target/configs/
  d21x_demo128-nand_rt-thread_usb_h264_display_defconfig
LICENSE                       Apache-2.0 许可证
windows-host/xfz1986-h264.patch  基于 ESP32 Windows IDD 项目的 H.264 改造
windows-host/README.md         上位机来源、应用补丁及构建说明
```

依赖 Luban-Lite SDK 内置的 CherryUSB 1.5.0、MPP H.264 解码器、AIC USB device/VE/显示驱动和 RT-Thread。本包使用 SDK 原有的 `LPKG_CHERRYUSB_DEVICE_WINUSB_SINGLE` 选项，由应用自身编译设备端源码，无需修改 CherryUSB 的 Kconfig 或 SConscript。示例配置要求 `CONFIG_LPKG_MPP=y`、`CONFIG_AIC_MPP_H264_DEC_ENABLE=y` 和 USB0、VE、显示相关选项。

## 集成到 Luban-Lite

在 SDK 根目录执行（把 `/path/to/usb-h264-display` 换成本包路径）：

```sh
cp -a /path/to/usb-h264-display/application/rt-thread/usb_h264_display application/rt-thread/
cp /path/to/usb-h264-display/target/configs/d21x_demo128-nand_rt-thread_usb_h264_display_defconfig target/configs/
scons --apply-def=d21x_demo128-nand_rt-thread_usb_h264_display_defconfig
scons -j4
```

若目标 SDK 中已有同名方案或应用，先比较文件再覆盖。构建前核对板卡的 Bootloader、屏幕面板和 USB device 口；缺失 Bootloader 时先编译相应的 `d21x_demo128-nand_baremetal_bootloader_defconfig`。最终镜像由 SDK 构建系统输出到 `output/`，本包本身不能脱离 SDK 独立编译。

移植其他分辨率时同步修改 `usbd_winusb_display.c` 的 `USB_DISPLAY_WIDTH`、`USB_DISPLAY_HEIGHT`、USB 产品字符串中的 `R1024x600`，以及板级显示配置。当前接收端只接受完整的 1024×600 帧，单个 H.264 access unit 最大 1 MiB，使用两个接收帧缓存；H.264 数据需为 Annex B 起始码格式。帧头为 16 字节，后面紧跟码流，格式见协议头。

## Windows 上位机

Windows 上位机**基于 [chuanjinpang 的 ESP32-S2 USB 扩展屏 Windows IDD 项目](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display) 改造**，不需要 ESP32 硬件。本包的 [H.264 补丁](windows-host/xfz1986-h264.patch)针对该上游项目的提交 `c2de0f56f778904d663d8e2dc5a71b4a6b270ecc`，增加 Windows Media Foundation H.264 编码、Annex B 输出、`Eh2644` 产品字符串解析，并将 INF USB ID 改为设备端的 `33C3:0E05`。具体应用和构建步骤见 [Windows 上位机说明](windows-host/README.md)。

上游原版只提供 RGB/JPEG 编码，原版已签名安装包不能直接驱动本 H.264 设备端。补丁已通过 `git apply --check` 和实际应用测试；**Windows 驱动编译、签名安装及真机联调尚未在本次打包中验证**。

## 已知范围

- 仅有 d21x `demo128-nand` 参考配置，未提供其他板卡的实测数据。
- 接收端不实现触摸回传、音频、动态分辨率或自动适配 Windows 驱动。
- 产品字符串、USB VID/PID 和分辨率当前写在设备端源码中；公开产品使用时应按硬件和 USB ID 授权情况调整。

## 许可与来源

本包中的设备端源码保留原始 `Copyright (c) 2026, ArtInChip Technology Co., Ltd` 与 `SPDX-License-Identifier: Apache-2.0` 标识，按仓库根目录的 `LICENSE` 发布。Windows 上位机补丁基于上述 ESP32 项目，来源和改动见 [Windows 上位机说明](windows-host/README.md)；本包不将设备端的 Apache-2.0 许可证扩展声明到上游 Windows 代码。CherryUSB、Luban-Lite SDK 和上游 Windows 项目各自遵循其许可条件。
