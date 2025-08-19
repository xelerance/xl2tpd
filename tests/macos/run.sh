set -e
d=$(dirname "$0")
for t in handshake_loopback.sh multi_sessions.sh ppp_loopback.sh; do
  bash "$d/$t"
done
