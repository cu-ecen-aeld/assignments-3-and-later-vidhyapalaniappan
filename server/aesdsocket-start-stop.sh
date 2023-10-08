#!/bin/sh

# Path to your aesdsocket executable
AESDSOCKET_EXEC=/path/to/your/aesdsocket

# Check if the executable exists
if [ ! -x "$AESDSOCKET_EXEC" ]; then
    echo "aesdsocket executable not found at $AESDSOCKET_EXEC"
    exit 1
fi

# Function to start the daemon
start() {
    echo "Starting aesdsocket daemon..."
    start-stop-daemon -b -S -x "$AESDSOCKET_EXEC" -- -d
}

# Function to stop the daemon
stop() {
    echo "Stopping aesdsocket daemon..."
    start-stop-daemon -K -x "$AESDSOCKET_EXEC"
}

# Check for the start or stop command
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0

