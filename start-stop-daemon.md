## init.d skeleton
```bash
### BEGIN INIT INFO
# Provides:          cgroup-metrics
# Required-Start:
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description: This is the cgroup metrics daemon
# Description:
### END INIT INFO
# Process name ( For display )
NAME=lacp-metrics
# Daemon name, where is the actual executable
DAEMON="/usr/bin/lacp_metrics.py"
# pid file for the daemon
PIDFILE=/run/lacp_metrics.pid
# directory for output
OUTPUTDIR=/run/prom/
# poll interval (seconds)
POLL_INTERVAL=30
# Arguments to the daemon
DAEMON_ARGS="--lacp-path=/host/proc/1/net/bonding --output-file=${OUTPUTDIR}/lacp_metrics.prom --poll ${POLL_INTERVAL}"
# If the daemon is not there, then exit.
test -x $DAEMON || exit 5

case $1 in
start)
    # Checked the PID file exists and check the actual status of process
    if [ -e $PIDFILE ]; then
        if pgrep -f $DAEMON > /dev/null; then
            echo "${NAME} already running"
            exit 0
        else
            rm $PIDFILE
        fi
    fi
    mkdir -p ${OUTPUTDIR}
    # Start the daemon.
    echo "Starting the process ${NAME}"
    # Start the daemon with the help of start-stop-daemon
    # Log the message appropriately
    if start-stop-daemon -S -q -b -C -p $PIDFILE -m -x $DAEMON -- $DAEMON_ARGS; then
        echo "${NAME} is running"
        exit
    else
        echo "Failed to start ${NAME}"
        exit 127
    fi
    ;;
stop)
    # Stop the daemon.
    if start-stop-daemon -K -q -p $PIDFILE -x $DAEMON; then
        echo "Stopped ${NAME}"
        rm $PIDFILE
        exit
    else
        echo "Failed to stop ${NAME}"
        exit 127
    fi
    ;;
restart)
    # Restart the daemon.
    $0 stop && sleep 2 && $0 start
    ;;
status)
    # Check the status of the process.
    if pgrep -f $DAEMON > /dev/null; then
        echo "$NAME is running"
        exit 0
    else
        echo "$NAME is stopped"
        exit 1
    fi
    ;;
esac
```

[init.d skeleton](https://gist.github.com/mrowe/8b617a8b12a6248d48b8)  
[init.d skeleton](https://git.yoctoproject.org/poky/plain/meta-skeleton/recipes-skeleton/service/service/skeleton)  
[start-stop-daemon](https://github.com/daleobrien/start-stop-daemon) #github  
[start-stop-daemon(8)](https://man7.org/linux/man-pages/man8/start-stop-daemon.8.html)  
