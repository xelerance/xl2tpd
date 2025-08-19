#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="$root/xl2tpd"
ctl="$root/xl2tpd-control"
work="$root/.ci-artifacts/macos/multi"
rm -rf "$work"
mkdir -p "$work"
sudo pkill xl2tpd 2>/dev/null || true
sudo mkdir -p /var/run/xl2tpd
sudo chown "$(id -u):$(id -g)" /var/run/xl2tpd
sudo rm -f /var/run/xl2tpd.pid
port_srv=51721
port_cli=51722
srv_cfg="$work/srv.conf"
cli_cfg="$work/cli.conf"
srv_ctl="$work/srv.ctrl"
cli_ctl="$work/cli.ctrl"
srv_log="$work/srv.log"
cli_log="$work/cli.log"
ppp_srv="$work/opt.lns"
ppp_cli="$work/opt.cli"
srv_pidf="$work/srv.pid"
cli_pidf="$work/cli.pid"
printf "noccp\nnoauth\nmtu 1200\nmru 1200\n" > "$ppp_srv"
printf "noccp\nnoauth\nmtu 1200\nmru 1200\n" > "$ppp_cli"
cat > "$srv_cfg" <<EOF2
[global]
port = $port_srv
listen-addr = 127.0.0.1
access control = no
debug packet = yes
debug state = yes

[lns default]
ip range = 10.77.0.2-10.77.0.20
local ip = 10.77.0.1
pppoptfile = $ppp_srv
length bit = yes
ppp debug = yes
 
EOF2
cat > "$cli_cfg" <<EOF3
[global]
port = $port_cli
listen-addr = 127.0.0.1
access control = no
debug packet = yes
debug state = yes

[lac dummy]
lns = 0.0.0.0
autodial = no

EOF3
"$bin" -D -c "$srv_cfg" -C "$srv_ctl" -p "$srv_pidf" >"$srv_log" 2>&1 &
srv_pid=$!
sleep 1
"$bin" -D -c "$cli_cfg" -C "$cli_ctl" -p "$cli_pidf" >"$cli_log" 2>&1 &
cli_pid=$!
sleep 1
n=5
i=1
while [ $i -le $n ]; do
  name="s$i"
  "$ctl" -c "$cli_ctl" add-lac "$name" lns=127.0.0.1:$port_srv pppoptfile="$ppp_cli"
  "$ctl" -c "$cli_ctl" connect-lac "$name" || true
  i=$((i+1))
done
deadline=$((SECONDS+20))
ok=0
while [ $SECONDS -lt $deadline ]; do
  cnt=$(grep -Eoc "SCCRQ|Start-Control-Connection-Request" "$srv_log" || true)
  if [ "$cnt" -ge "$n" ]; then
    ok=1
    break
  fi
  sleep 1
done
kill "$cli_pid" || true
kill "$srv_pid" || true
wait || true
test "$ok" -eq 1
