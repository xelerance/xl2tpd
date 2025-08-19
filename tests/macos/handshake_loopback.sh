#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="$root/xl2tpd"
ctl="$root/xl2tpd-control"
work="$root/.ci-artifacts/macos/handshake"
rm -rf "$work"
mkdir -p "$work"
sudo pkill xl2tpd 2>/dev/null || true
sudo mkdir -p /var/run/xl2tpd
sudo chown "$(id -u):$(id -g)" /var/run/xl2tpd
sudo rm -f /var/run/xl2tpd.pid
port_srv=51701
port_cli=51702
srv_cfg="$work/xl2tpd-server.conf"
cli_cfg="$work/xl2tpd-client.conf"
srv_ctl="$work/server.control"
cli_ctl="$work/client.control"
srv_log="$work/server.log"
cli_log="$work/client.log"
ppp_srv="$work/options.l2tpd.lns"
ppp_cli="$work/options.l2tpd.client"
srv_pidf="$work/server.pid"
cli_pidf="$work/client.pid"
printf "lcp-echo-interval 5\nlcp-echo-failure 3\nnoccp\nnoauth\nmtu 1280\nmru 1280\n" > "$ppp_srv"
printf "lcp-echo-interval 5\nlcp-echo-failure 3\nnoccp\nnoauth\nmtu 1280\nmru 1280\n" > "$ppp_cli"
cat > "$srv_cfg" <<EOF2
[global]
port = $port_srv
listen-addr = 127.0.0.1
access control = no
debug packet = yes
debug state = yes
debug tunnel = yes

[lns default]
ip range = 10.99.0.2-10.99.0.10
local ip = 10.99.0.1
ppp debug = yes
pppoptfile = $ppp_srv
length bit = yes
 
EOF2
cat > "$cli_cfg" <<EOF3
[global]
port = $port_cli
listen-addr = 127.0.0.1
access control = no
debug packet = yes
debug state = yes
debug tunnel = yes

[lac loop]
lns = 127.0.0.1:$port_srv
ppp debug = yes
pppoptfile = $ppp_cli
autodial = no

EOF3
"$bin" -D -c "$srv_cfg" -C "$srv_ctl" -p "$srv_pidf" >"$srv_log" 2>&1 &
srv_pid=$!
sleep 1
"$bin" -D -c "$cli_cfg" -C "$cli_ctl" -p "$cli_pidf" >"$cli_log" 2>&1 &
cli_pid=$!
sleep 1
"$ctl" -c "$cli_ctl" connect-lac loop || true
deadline=$((SECONDS+15))
ok=0
while [ $SECONDS -lt $deadline ]; do
  if grep -qE "SCCRQ|SCCRP|SCCCN" "$srv_log"; then
    ok=1
    break
  fi
  sleep 1
done
kill "$cli_pid" || true
kill "$srv_pid" || true
wait || true
test "$ok" -eq 1
