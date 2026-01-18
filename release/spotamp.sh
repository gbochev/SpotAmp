#!/bin/sh

cleanup() {
    echo "Stopping spotamp and librespot..."
    pkill -TERM -P "$server_pid" 2>/dev/null
    pkill -TERM -P "$spotamp_pid" 2>/dev/null
    kill -TERM "$server_pid" 2>/dev/null
    kill -TERM "$spotamp_pid" 2>/dev/null
    exit 0
}

trap cleanup INT TERM

# Start librespot in background
./go-librespot --config_dir . &
server_pid=$!

# Start spotamp in background
./spotamp &
spotamp_pid=$!

echo "librespot PID: $server_pid, spotamp PID: $spotamp_pid"

# Monitor both in a loop
while true; do
    sleep 1
    if ! kill -0 "$server_pid" 2>/dev/null; then
        echo "librespot exited, killing spotamp..."
        kill -TERM "$spotamp_pid" 2>/dev/null
        break
    fi
    if ! kill -0 "$spotamp_pid" 2>/dev/null; then
        echo "spotamp exited, killing librespot..."
        kill -TERM "$server_pid" 2>/dev/null
        break
    fi
done

wait