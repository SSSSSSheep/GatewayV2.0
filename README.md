# GatewayV2.0

## 运行配置

`main_demo` 启动时读取以下环境变量；未设置或设置为空时使用默认值。修改环境变量后重启程序即可生效，不需要重新编译。

| 环境变量 | 用途 | 默认值 |
| --- | --- | --- |
| `GATEWAY_BROKER_URL` | MQTT Broker 地址 | `tcp://192.168.0.105:1883` |
| `GATEWAY_SERIAL_DEVICE` | 串口设备 | `/dev/ttyS3` |
| `GATEWAY_MQTT_CLIENT_ID` | MQTT 客户端 ID | `rk_gateway_v2_pc` |
| `GATEWAY_MQTT_DOWN_TOPIC` | 接收下行消息的主题 | `gateway/down` |
| `GATEWAY_MQTT_UP_TOPIC` | 发布上行消息的主题 | `gateway/up` |

串口格式固定为 115200 8N1。多个网关同时连接同一个 Broker 时，应为每个网关设置不同的 `GATEWAY_MQTT_CLIENT_ID`。

板端手动启动示例：

```bash
GATEWAY_BROKER_URL=tcp://192.168.0.105:1883 GATEWAY_SERIAL_DEVICE=/dev/ttyS3 GATEWAY_MQTT_CLIENT_ID=rk_gateway_v2_pc GATEWAY_MQTT_DOWN_TOPIC=gateway/down GATEWAY_MQTT_UP_TOPIC=gateway/up /tmp/main_demo
```

## 构建与测试

PC 单元测试：`make pc-test`。

RK3568 交叉编译：`make rk-check`。

板端集成测试脚本会交叉编译并上传程序、订阅上行主题、运行板端程序并拉取日志：

```bash
RK_HOST=cat@192.168.0.107 MQTT_HOST=192.168.0.105 scripts/test-rk-uplink-10s.sh
```

脚本支持 `RK_HOST`、`MQTT_HOST`、`MQTT_PORT`、`MQTT_TOPIC` 和上述 `GATEWAY_*` 环境变量；设置 `GATEWAY_MQTT_UP_TOPIC` 时，测试脚本会同步订阅该主题。日志默认保存在 `build/rk-uplink-10s-gateway.log` 和 `build/rk-uplink-10s-broker.log`。
