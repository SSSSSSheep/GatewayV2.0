# GatewayV2.0

## 运行配置

`main_demo` 启动时读取以下环境变量；未设置或设置为空时使用默认值。修改环境变量后重启程序即可生效，不需要重新编译。下表 IP 是当前联调网络的地址，部署到其他网络时应替换。

| 环境变量 | 用途 | 默认值 |
| --- | --- | --- |
| `GATEWAY_BROKER_URL` | MQTT Broker 地址；TLS 使用 `ssl://` | `ssl://192.168.0.105:8883` |
| `GATEWAY_SERIAL_DEVICE` | 串口设备 | `/dev/ttyS3` |
| `GATEWAY_MQTT_CLIENT_ID` | MQTT 客户端 ID；每台网关必须唯一 | `rk_gateway_v2_103` |
| `GATEWAY_MQTT_USERNAME` | MQTT 用户名 | `rk_gateway` |
| `GATEWAY_MQTT_PASSWORD` | MQTT 密码；不要提交真实密码 | 空，部署时填写 |
| `GATEWAY_MQTT_CA_FILE` | 验证 Broker 证书的 CA 文件 | `/etc/ssl/certs/rk-gateway-ca.crt` |
| `GATEWAY_MQTT_DOWN_TOPIC` | 接收下行消息的主题 | `gateway/down` |
| `GATEWAY_MQTT_UP_TOPIC` | 发布上行消息的主题 | `gateway/up` |

串口格式固定为 115200 8N1。多个网关同时连接同一个 Broker 时，应为每个网关设置不同的 `GATEWAY_MQTT_CLIENT_ID`。

当前联调网络为 Windows Broker `192.168.0.105`、Ubuntu `192.168.0.106`、RK3568 `192.168.0.103`。静态 IP 可能变化；修改 Broker IP 时，Broker 服务器证书的 SAN 也必须包含连接所用的 IP。

## TLS 与认证

网关生产配置应使用 TLS listener `8883`、专用 MQTT 账号及只读 CA 证书。不要将 Broker 私钥、CA 私钥或实际 MQTT 密码提交到仓库。网关仅需要 CA 公钥证书验证服务器；Mosquitto 的 `server.key` 和 CA 私钥只留在 Broker 主机。

将 Broker 的 CA 证书安全复制到 RK3568，然后安装为系统可读文件：

```bash
scp rk-gateway-ca.crt cat@192.168.0.103:/tmp/rk-gateway-ca.crt
sudo install -o root -g root -m 0644 /tmp/rk-gateway-ca.crt /etc/ssl/certs/rk-gateway-ca.crt
```

复制部署环境示例后，编辑 `/etc/default/rk-gateway`，设置 Broker IP、`rk_gateway` 密码及 CA 路径。该文件含明文 MQTT 密码，应限制为 root 可读：

```bash
sudo install -o root -g root -m 0600 /tmp/rk-gateway.env /etc/default/rk-gateway
sudo vim /etc/default/rk-gateway
```

保存后检查权限并重启服务：

```bash
sudo chmod 600 /etc/default/rk-gateway
sudo systemctl restart rk-gateway.service
systemctl is-active rk-gateway.service
sudo journalctl -u rk-gateway.service -n 30 --no-pager
```

首次验证 TLS 时，可从 RK3568 检查 Broker CA 和 IP SAN：

```bash
openssl s_client -connect 192.168.0.105:8883 \
  -CAfile /etc/ssl/certs/rk-gateway-ca.crt \
  -verify_ip 192.168.0.105 -verify_return_error </dev/null
```

验证结果应包含 `Verify return code: 0 (ok)`。换 Broker IP 后应使用新的地址执行验证。

板端临时手动启动示例（已先设置环境变量中的密码，不建议将真实密码写入 shell 历史）：

```bash
GATEWAY_BROKER_URL=ssl://192.168.0.105:8883 \
GATEWAY_SERIAL_DEVICE=/dev/ttyS3 \
GATEWAY_MQTT_CLIENT_ID=rk_gateway_v2_103 \
GATEWAY_MQTT_USERNAME=rk_gateway \
GATEWAY_MQTT_CA_FILE=/etc/ssl/certs/rk-gateway-ca.crt \
/tmp/main_demo
```

## 构建与测试

PC 单元测试：`make pc-test`。

RK3568 交叉编译：`make rk-check`。

板端集成测试脚本会交叉编译并上传程序、订阅上行主题、运行板端程序并拉取日志：

```bash
RK_HOST=cat@192.168.0.103 MQTT_HOST=192.168.0.105 scripts/test-rk-uplink-10s.sh
```

脚本支持 `RK_HOST`、`MQTT_HOST`、`MQTT_PORT`、`MQTT_TOPIC` 和上述 `GATEWAY_*` 环境变量；设置 `GATEWAY_MQTT_UP_TOPIC` 时，测试脚本会同步订阅该主题。日志默认保存在 `build/rk-uplink-10s-gateway.log` 和 `build/rk-uplink-10s-broker.log`。

## 板端 systemd 部署

从 Ubuntu 工程目录将程序和部署文件上传到板端：

```bash
scp build/rk3568-aarch64-sysroot/main_demo cat@192.168.0.103:/tmp/main_demo
scp deploy/rk-gateway.service cat@192.168.0.103:/tmp/rk-gateway.service
scp deploy/rk-gateway.env.example cat@192.168.0.103:/tmp/rk-gateway.env
```

在板端安装文件并启用服务：

```bash
sudo install -m 0755 /tmp/main_demo /usr/local/bin/rk-gateway
sudo install -o root -g root -m 0600 /tmp/rk-gateway.env /etc/default/rk-gateway
sudo install -m 0644 /tmp/rk-gateway.service /etc/systemd/system/rk-gateway.service
sudo systemctl daemon-reload
sudo systemctl enable --now rk-gateway.service
```

查看运行状态和实时日志：

```bash
systemctl is-enabled rk-gateway.service
systemctl is-active rk-gateway.service
sudo journalctl -fu rk-gateway.service
```

管理服务：`sudo systemctl stop rk-gateway.service` 停止，`sudo systemctl start rk-gateway.service` 启动。当前 unit 使用 `Restart=on-failure`，并通过 `stdbuf` 将程序日志实时写入 journal。部署到其他设备时，修改环境文件中的串口、Broker、主题，并为每台网关设置不同的 Client ID。

## Web 控制台

控制台通过浏览器连接 Mosquitto WebSocket listener `8083`，当前 Broker 配置允许该 listener 匿名访问。它与网关使用账号认证的 TLS listener `8883` 是不同入口；WebSocket 连接不继承 `rk_gateway` ACL。不要将 `8083` 暴露到不可信网络。控制台的发布成功提示仅代表浏览器 MQTT 客户端完成发布回调，不代表下位机执行成功。

## STM32 下位机

Keil 工程、构建与烧录步骤见 [`node_stm32/README.md`](node_stm32/README.md)。

## MQTT 消息投递边界

上下行 MQTT 当前均使用 QoS 0，不要求 Broker 确认收到。上行消息先进入容量为 64 条的内存队列；队列满时拒收新消息。遇到 MQTT 断连错误时，发布线程会在内存中保留当前消息并重连后重试；其他发布错误只记录日志，该消息不会重试。进程退出或崩溃时，尚未发布的消息不会写入磁盘，因此可能丢失。此策略适用于允许丢样的周期遥测，不保证命令或告警可靠送达。
