#! /bin/bash

SCRIPT_DIR=$(dirname "$0")

BUILD_DIR=$1

cat > test_config.json << EOF
{
  "options": {},
  "outdir": "./reports/",
  "servers": [
    {
      "agent": "CAF Websocket Autobahn Driver",
      "url": "ws://localhost:7788"
    }
  ],
  "cases": [ "*" ],
  "exclude-cases": [],
  "exclude-agent-cases": {}
}
EOF

"${BUILD_DIR}"/libcaf_net/caf-net-autobahn-driver -p 7788 >/dev/null &

wstest -m fuzzingclient -s test_config.json

kill %1
