#! /bin/bash -x

set -e

REPORT_DIR="./reports"
REPORT_JSON_FILE="$REPORT_DIR/index.json"
CONFIG_FILE="test_config.json"

function cleanup() {
  rm -rf "$CONFIG_FILE"
  kill %1
}

function count_by_status() {
    GREP_STRING=$(printf '"behavior": "%s"' "$1")
    NUM=$(grep -c "$GREP_STRING" $REPORT_JSON_FILE)
    if [ "$NUM" -gt 0 ]; then 
        echo "There are $NUM $1 Autobahn tests" >&2
        echo "The following tests finished with status $1" >&2
        grep "$GREP_STRING" -B1 "$REPORT_JSON_FILE" |\
            grep -v -P "(behavior)|(--)" |\
            cut -d: -f 1 >&2
    fi
    echo "$NUM"
}

if [ $# -ne 1 ]; then 
  exit 255
fi

cat > $CONFIG_FILE << EOF
{
  "options": {},
  "outdir": "$REPORT_DIR",
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
trap cleanup EXIT

set -x
"$BUILD_DIR"/libcaf_net/caf-net-autobahn-driver -p 7788 >/dev/null &
wstest -m fuzzingclient -s $CONFIG_FILE

echo "Autobahn testsuite finished"
echo "Test report by status:"
grep '"behavior"' $REPORT_JSON_FILE | sort | uniq -c

NUM_OF_FAILED_TESTS=$(count_by_status FAILED)
NUM_OF_NON_STRICT=$(count_by_status NON-STRICT)
exit $((NUM_OF_NON_STRICT + NUM_OF_FAILED_TESTS))
