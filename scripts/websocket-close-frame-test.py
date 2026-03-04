#!/usr/bin/env python3
"""
WebSocket close frame test client.

Connects to a WebSocket server, sends a message, sends a close frame, then
collects and counts all incoming frames until the connection closes. Useful for
detecting bugs where the server sends multiple close frames instead of one.

Usage:
    python scripts/websocket-close-frame-test.py <URL>
"""

import sys

try:
    import websocket
    from websocket import ABNF
except ImportError:
    print("Error: websocket-client is required. Install with: pip install websocket-client", file=sys.stderr)
    sys.exit(2)


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: websocket-close-frame-test.py <URL>", file=sys.stderr)
        sys.exit(2)
    url = sys.argv[1]
    ws = websocket.WebSocket()
    ws.settimeout(5.0)
    try:
        print(f"Connecting to {url}...")
        ws.connect(url)
        msg = "Hello"
        print(f"Sending message: {msg!r}")
        ws.send(msg)
        try:
            opcode, data = ws.recv_data(control_frame=True)
            if opcode == ABNF.OPCODE_TEXT:
                print(f"Received echo: {data!r}")
            elif opcode == ABNF.OPCODE_BINARY:
                print(f"Received binary: {data!r}")
        except websocket.WebSocketConnectionClosedException:
            # Connection may have been closed already; ignore.
            pass
        except Exception as e:
            print(f"Unexpected error while receiving initial frame: {e}", file=sys.stderr)
            raise
        # Send close frame
        print("Sending close frame...")
        ws.send_close(status=1000, reason=b"Client closing")
        # Collect all incoming frames until connection closes
        close_frames = []
        all_frames = []
        opcode_names = {
            ABNF.OPCODE_TEXT: "text",
            ABNF.OPCODE_BINARY: "binary",
            ABNF.OPCODE_CLOSE: "close",
            ABNF.OPCODE_PING: "ping",
            ABNF.OPCODE_PONG: "pong",
            ABNF.OPCODE_CONT: "continuation",
        }
        print("Receiving frames (waiting for close)...")
        while True:
            try:
                frame = ws.recv_frame()
                all_frames.append(frame)
                opcode_name = opcode_names.get(frame.opcode, f"unknown({frame.opcode})")
                if frame.opcode == ABNF.OPCODE_CLOSE:
                    close_frames.append(frame)
                    payload = frame.data
                    if len(payload) >= 2:
                        status = (payload[0] << 8) | payload[1]
                        reason = payload[2:].decode("utf-8", errors="replace") if len(payload) > 2 else ""
                        print(f"  Close frame #{len(close_frames)}: status={status}, reason={reason!r}")
                    else:
                        print(f"  Close frame #{len(close_frames)}: (no status/reason)")
                else:
                    print(f"  {opcode_name} frame: {len(frame.data)} bytes")
            except websocket.WebSocketConnectionClosedException:
                break
            except Exception as e:
                # Connection closed or timeout
                err_str = str(e).lower()
                if "timed out" in err_str or "timeout" in err_str:
                    break
                raise

        # Report results
        print(f"Total frames received after sending close: {len(all_frames)}")
        print(f"Close frames received: {len(close_frames)}")
        return len(close_frames) != 1

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    finally:
        try:
            ws.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
