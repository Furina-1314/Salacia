# CamStream —— 板端视频采集与硬件编码推流

运行于 STM32MP257 Cortex-A35（OpenSTLinux）的图传源程序：摄像头输出的 RGB24 帧经 **OpenCL（GPU）色彩空间转换**为 NV12，注入 GStreamer 管线由片上 **V4L2 硬件 H.264 编码器**（`v4l2slh264enc`）编码，封装 RTP over UDP 推向岸上终端。1080p / 720p 双模式共用同一核心库与 OpenCL 内核，是 Salacia 实时图传链路的板端起点。

## 在系统中的位置

```text
摄像头(MIPI) → DCMIPP/ISP → V4L2采集 → OpenCL转换 → 硬件H.264 → RTP/UDP → Wi-Fi浮标 → 岸上终端
                                        本工程                         └── Gateway_A35 编排 ──┘
```

生产部署中由 `Gateway_A35/deploy/rov_camstream.service`（脚本 `rov_camstream.sh`，gst-launch + libcamera 管线）作为 systemd 服务编排启停，参数来自 `/etc/rov_gateway.ini [camstream]`；本工程为等价的自研直采实现（绕过 libcamera，直接 V4L2/DCMIPP + OpenCL），用于开发调试与链路对照。二者共用同一 RTP/UDP 接收约定。

## 视频管线与帧率策略

| 模式 | 分辨率 | 默认码率 | 帧率行为 |
|---|---|---|---|
| `rgb_1080p` | 1920×1080 | 4000 kbps | 传感器默认 30fps 供帧，处理链实测稳定 ~23fps；**不降帧策略**：多余帧在驱动队列源头丢弃，不积压不迟滞 |
| `rgb_720p` | 1280×720 | 3000 kbps | 传感器仍按 1080p 节拍运行，postproc **硬件缩放**至 720p@30fps |

要点：

- **色彩转换在 GPU 上完成**：`rgb_to_nv12.cl` OpenCL 内核，CPU 不参与逐像素搬运；
- **编码在硬件编码器完成**：CPU 占用极低，为网关/传感器汇聚留出算力；
- **ISP 自动化**：STREAMON 后自动应用色彩 profile（默认 `tl84` 水下场景常用），支持增益/曝光/对比度调节与帧转储（`--dump-*`）用于现场排查；
- vblank 帧率控制在 STREAMON 前写入（流状态下写入不生效；vblank 跨运行持久，启动时统一复位/设置）；
- Ctrl-C 退出并完整清理资源。

### 核心模块（`core/` 共享库，Phase 1 自 920 行 main 拆分）

| 模块 | 职责 |
|---|---|
| `dcmipp_setup` | DCMIPP 媒体管线配置 |
| `v4l2_capture` | V4L2 采集（RGB24） |
| `cl_converter` | OpenCL GPU 色彩转换（`rgb_to_nv12.cl`） |
| `gst_streamer` | GStreamer 管线组装与 RTP 推流（appsrc → v4l2slh264enc） |
| `isp_controller` | ISP profile 加载与应用 |
| `sensor_control` / `camera_control` | 传感器增益/曝光/vblank 与相机状态控制 |
| `camera_config` / `stream_app` | 参数解析（AppConfig）与主流程组装 |
| `control_console` / `status_printer` / `frame_dumper` | 运行中控制台、状态打印与调试帧转储 |

## 构建

板端（OpenSTLinux，需 OpenCL 与 GStreamer 开发包）：

```bash
cd CamStream && mkdir -p build && cd build
cmake .. && make -j$(nproc)
# 产出 rgb_1080p / rgb_720p
```

## 使用

```bash
./rgb_1080p [采集设备] [目标IP] [端口] [码率kbps] [帧率] [命名参数...]
./rgb_720p  [采集设备] [目标IP] [端口] [码率kbps] [帧率] [命名参数...]
```

默认值：`/dev/video1 192.168.1.100 5000 4000(720p为3000) 0`。**板端默认目标是 192.168.1.100，主机 IP 变化时务必显式传入。**

命名参数：

| 参数 | 默认 | 说明 |
|---|---|---|
| `--sensor-gain N` | 35 | 传感器增益 |
| `--sensor-exposure N` | 2000 | 传感器曝光 |
| `--isp-profile tl84\|d50\|none` | tl84 | ISP 色彩 profile（STREAMON 后自动应用） |
| `--isp-contrast none\|50\|200\|dynamic` | none | ISP 对比度 |
| `--require-isp` | 关 | ISP 不可用时强制失败 |
| `--input-mode hostptr\|copy` | — | 帧输入模式 |
| `--gop N` / `--colorimetry bt601\|bt709` | — | 关键帧间隔 / 色彩空间声明 |
| `--dump-frame N` / `--dump-every N` / `--dump-count M` | — | 调试帧转储 |

示例（目标为岸上主机 192.168.1.100）：

```bash
./rgb_1080p /dev/video1 192.168.1.100 5000 4000
```

## 接收端

- **Salacia_Terminal**：RTP/H264 over UDP 直收（SPS/PPS 每 1s 带内重发、关键帧间隔 1s，迟入 ≤1s 自动同步，无需 RTSP）；首次真机对接需放行 Windows 防火墙入站 UDP。
- **VLC（调试）**：将板端生成的 `stream_1080p.sdp` / `stream_720p.sdp` 拷至主机后：

```bash
vlc --network-caching=100 stream_1080p.sdp
```

## 注意事项

- 推流前先确认板端与主机可互相 ping 通；
- 视频与控制分属 UDP/TCP 两条链路，本工程不占用 TCP 7000（网关专用）；
- 网关运行期间 `/dev/ttyRPMSG0` 由网关独占，与本工程无关，但不得并发第二个视频进程写同一目标端口。
