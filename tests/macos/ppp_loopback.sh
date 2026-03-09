#!/usr/bin/env bash
set -euo pipefail
if [ "${CI_ALLOW_PPP_E2E:-0}" != "1" ]; then
  exit 0
fi
if ! command -v pppd >/dev/null 2>&1; then
  exit 0
fi
if ! sudo -n true 2>/dev/null; then
  exit 0
fi
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="$root/xl2tpd"
ctl="$root/xl2tpd-control"
work="$root/.ci-artifacts/macos/ppp"
rm -rf "$work"
mkdir -p "$work"
sudo pkill xl2tpd 2>/dev/null || true
sudo mkdir -p /var/run/xl2tpd
sudo rm -f /var/run/xl2tpd.pid
port_srv=51711
port_cli=51712
srv_cfg="$work/srv.conf"
cli_cfg="$work/cli.conf"
srv_ctl="$work/server.control"
cli_ctl="$work/client.control"
srv_log="$work/server.log"
cli_log="$work/client.log"
ppp_srv="$work/options.l2tpd.lns"
ppp_cli="$work/options.l2tpd.client"
srv_pidf="$work/srv.pid"
cli_pidf="$work/cli.pid"
sudo mkdir -p /etc/ppp
printf "\"client\"\t\"lns\"\t\"testpass\"\t\"*\"\n" | sudo tee /etc/ppp/chap-secrets >/dev/null
printf "name lns\nrequire-mschap-v2\nnoccp\nnodefaultroute\nmtu 1280\nmru 1280\nlcp-echo-interval 5\nlcp-echo-failure 3\n" > "$ppp_srv"
printf "user client\npassword testpass\nremotename lns\nnoauth\nipcp-accept-local\nipcp-accept-remote\nmtu 1280\nmru 1280\nlcp-echo-interval 5\nlcp-echo-failure 3\n" > "$ppp_cli"
cat > "$srv_cfg" <<EOF2
[global]
port = $port_srv
listen-addr = 127.0.0.1
access control = no

[lns default]
ip range = 10.66.0.2-10.66.0.10
local ip = 10.66.0.1
pppoptfile = $ppp_srv
ppp debug = yes
length bit = yes
refuse pap = yes
 
EOF2
cat > "$cli_cfg" <<EOF3
[global]
port = $port_cli
listen-addr = 127.0.0.1

[lac loop]
lns = 127.0.0.1:$port_srv
pppoptfile = $ppp_cli
ppp debug = yes
autodial = no

EOF3
sudo "$bin" -D -c "$srv_cfg" -C "$srv_ctl" -p "$srv_pidf" >"$srv_log" 2>&1 &
srv_pid=$!
sleep 1
sudo "$bin" -D -c "$cli_cfg" -C "$cli_ctl" -p "$cli_pidf" >"$cli_log" 2>&1 &
cli_pid=$!
sleep 1
sudo "$ctl" -c "$cli_ctl" connect-lac loop
deadline=$((SECONDS+30))
ok=0
while [ $SECONDS -lt $deadline ]; do
  if ifconfig -l | tr ' ' '\n' | grep -q '^ppp'; then
    ok=1
    break
  fi
  sleep 1
done
if [ $ok -eq 1 ]; then
  ping -c 1 10.66.0.1 || true
fi
sudo kill "$cli_pid" || true
sudo kill "$srv_pid" || true
wait || true
test "$ok" -eq 1
