#!/usr/bin/env bash
set -Eeuo pipefail

RK_HOST="${RK_HOST:-cat@192.168.0.107}"
RK_LOCAL_BINARY="${RK_LOCAL_BINARY:-build/rk3568-aarch64-sysroot/main_demo}"
RK_REMOTE_BINARY="/tmp/main_demo"
RK_REMOTE_LOG="/tmp/gateway.log"
LOCAL_LOG="${LOCAL_LOG:-build/rk-uplink-10s-gateway.log}"
MQTT_LOG="${MQTT_LOG:-build/rk-uplink-10s-broker.log}"
MQTT_HOST="${MQTT_HOST:-192.168.0.105}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_TOPIC="${MQTT_TOPIC:-${GATEWAY_MQTT_UP_TOPIC:-gateway/up}}"
# 将测试脚本配置同步传给板端网关进程。
GATEWAY_BROKER_URL="${GATEWAY_BROKER_URL:-tcp://${MQTT_HOST}:${MQTT_PORT}}"
GATEWAY_SERIAL_DEVICE="${GATEWAY_SERIAL_DEVICE:-/dev/ttyS3}"
GATEWAY_MQTT_CLIENT_ID="${GATEWAY_MQTT_CLIENT_ID:-rk_gateway_v2_pc}"
GATEWAY_MQTT_DOWN_TOPIC="${GATEWAY_MQTT_DOWN_TOPIC:-gateway/down}"
GATEWAY_MQTT_UP_TOPIC="${GATEWAY_MQTT_UP_TOPIC:-$MQTT_TOPIC}"
DOWNLINK_JSON='{"conn_type":1,"id":"0002","msg":"30"}'
printf -v REMOTE_GATEWAY_ENV "GATEWAY_BROKER_URL=%q GATEWAY_SERIAL_DEVICE=%q GATEWAY_MQTT_CLIENT_ID=%q GATEWAY_MQTT_DOWN_TOPIC=%q GATEWAY_MQTT_UP_TOPIC=%q" "$GATEWAY_BROKER_URL" "$GATEWAY_SERIAL_DEVICE" "$GATEWAY_MQTT_CLIENT_ID" "$GATEWAY_MQTT_DOWN_TOPIC" "$GATEWAY_MQTT_UP_TOPIC"

for cmd in make sshpass ssh scp mosquitto_sub mosquitto_pub timeout; do
    command -v "$cmd" >/dev/null 2>&1 || {
        printf '缺少命令：%s\n' "$cmd" >&2
        exit 1
    }
done

if [[ -z "${SSHPASS:-}" ]]; then
    read -rsp "输入 RK3568 的 SSH 密码: " SSHPASS
    printf '\n'
    export SSHPASS
fi

if [[ -z "$SSHPASS" ]]; then
    printf 'SSH 密码不能为空\n' >&2
    exit 1
fi

SSH_OPTS=(
    -o ConnectTimeout=8
    -o ServerAliveInterval=3
    -o ServerAliveCountMax=2
    -o StrictHostKeyChecking=accept-new
)

SUB_PID=""
PUB_PID=""
cleanup() {
    if [[ -n "$PUB_PID" ]]; then
        kill "$PUB_PID" 2>/dev/null || true
        wait "$PUB_PID" 2>/dev/null || true
    fi
    if [[ -n "$SUB_PID" ]]; then
        kill "$SUB_PID" 2>/dev/null || true
        wait "$SUB_PID" 2>/dev/null || true
    fi
    unset SSHPASS
}
trap cleanup EXIT

printf '== 交叉编译并检查架构 ==\n'
make rk-check

if [[ ! -x "$RK_LOCAL_BINARY" ]]; then
    printf '找不到可执行文件：%s\n' "$RK_LOCAL_BINARY" >&2
    printf '请检查 RK_LOCAL_BINARY 是否与 Makefile 的构建目录一致。\n' >&2
    exit 1
fi

mkdir -p "$(dirname "$LOCAL_LOG")"
mkdir -p "$(dirname "$MQTT_LOG")"

if command -v mosquitto_sub >/dev/null 2>&1 && command -v timeout >/dev/null 2>&1; then
    : > "$MQTT_LOG"
    timeout 60s mosquitto_sub \
        -h "$MQTT_HOST" -p "$MQTT_PORT" -t "$MQTT_TOPIC" -v \
        >"$MQTT_LOG" 2>&1 &
    SUB_PID=$!
    sleep 1
else
    printf '未找到 mosquitto_sub 或 timeout；将只检查板端日志。\n'
fi

printf '== 上传二进制到 %s:%s ==\n' "$RK_HOST" "$RK_REMOTE_BINARY"
sshpass -e scp "${SSH_OPTS[@]}" \
    "$RK_LOCAL_BINARY" "$RK_HOST:$RK_REMOTE_BINARY"

printf '== 板端运行 10 秒并保存日志 ==\n'
(sleep 3; mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" -t "$GATEWAY_MQTT_DOWN_TOPIC" -m "$DOWNLINK_JSON") &
PUB_PID=$!
sshpass -e ssh "${SSH_OPTS[@]}" "$RK_HOST" "env $REMOTE_GATEWAY_ENV sh -s" <<'REMOTE'
set -eu

BIN=/tmp/main_demo
LOG=/tmp/gateway.log

if pgrep -x main_demo >/dev/null 2>&1; then
    printf '已有 main_demo 在运行。请先在 Xshell 中停止它，再重试。\n' >&2
    exit 2
fi

if ! command -v stdbuf >/dev/null 2>&1; then
    printf '板端缺少 stdbuf，无法保证日志及时写入文件。\n' >&2
    exit 3
fi

: > "$LOG"
stdbuf -oL -eL "$BIN" >"$LOG" 2>&1 </dev/null &
pid=$!

sleep 10

if kill -0 "$pid" 2>/dev/null; then
    kill -TERM "$pid" 2>/dev/null || true
fi
wait "$pid"
printf '板端测试结束，日志保存在 %s\n' "$LOG"
REMOTE

if ! wait "$PUB_PID"; then
    printf "MQTT 下行测试消息发布失败" >&2
    exit 1
fi
PUB_PID=""
printf '== 拉取板端日志 ==\n'
sshpass -e scp "${SSH_OPTS[@]}" \
    "$RK_HOST:$RK_REMOTE_LOG" "$LOCAL_LOG"

printf '\n板端日志：%s\n' "$LOCAL_LOG"
grep -Fq "MQTT收到主题：$GATEWAY_MQTT_DOWN_TOPIC" "$LOCAL_LOG" || { printf "板端未收到下行主题" >&2; exit 1; }
grep -Fq "串口写入：16/16 字节" "$LOCAL_LOG" || { printf "板端未完整写出串口帧" >&2; exit 1; }
grep -Fq "收到退出信号，正在清理资源" "$LOCAL_LOG" || { printf "板端未正常记录退出清理" >&2; exit 1; }
grep -Fq "$GATEWAY_MQTT_UP_TOPIC " "$MQTT_LOG" || { printf "Broker 未收到预期上行主题" >&2; exit 1; }
echo "上下行集成测试通过"
if [[ -f "$MQTT_LOG" ]]; then
    printf 'Broker 收到的上行消息：%s\n' "$MQTT_LOG"
fi
