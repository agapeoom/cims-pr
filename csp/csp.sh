#!/bin/sh
#------------------------------------------------------------------------------
# csp start/stop script
#------------------------------------------------------------------------------
# programmer : yee young han ( websearch@naver.com )
#------------------------------------------------------------------------------
# start date : 2012/09/14
#------------------------------------------------------------------------------

# program directory
# Get the absolute path of the script's directory
root_dir=$(cd "$(dirname "$0")" && pwd)
bin_dir="$root_dir/build"
program="csp"
program_list="$program"
# Use the config file in the build directory
program_arg="csp.xml"
program_name="csp"
logfile="log/cspdog.log"
this_script="csp.sh"

# get process pid
getpids(){
        # Use pgrep for robust pid finding
        if [ "$1" = "csp" ]; then
            # Exact match for the binary name to avoid matching this script
            pgrep -x "$1"
        else
            # Full command line match for watchdog script
            pgrep -f "$1"
        fi
}

# start function
start() {
        if [ -n "`getpids $program`" ]; then
                echo "$program_name: already running"
        else
                echo "$program_name: Starting..."
                ulimit -c unlimited
                ulimit -n 65535

                start_program
                cd $root_dir
        fi
}

# stop function
stop() {
        echo "$program_name : Stopping... "
        # Try pkill if killall fails or just use kill on pids
        pids=$(getpids $program)
        if [ -n "$pids" ]; then
            kill $pids
            echo "Sent kill signal to $pids"
        else
            echo "No process found to stop"
        fi
}

# status function
status() {
        pids=`getpids $program`
        if [ "$pids" != "" ]; then
                echo "$program_name: running ($pids)"
        else
                echo "$program_name: not running"
        fi

        pids=`getpids "$this_script watchdog"`
        if [ "$pids" != "" ]; then
                echo "watchdog: running ($pids)"
        else
                echo "watchdog: not running"
        fi
}

start_program(){
        d=`date +%Y/%m/%d_%H:%M:%S`

        echo "[$d] $1: Starting..." >> $logfile
        ulimit -c unlimited
        ulimit -n 65535
        cd $bin_dir
        $bin_dir/$program $program_arg
        cd $root_dir
}

watchdog(){
        echo "watchdog: Starting..."
        sleep 10
        while [ 1 ]; do
                for program in $program_list
                do
                        if [ -f "$bin_dir/$program" ]; then
                                if [ "`getpids $program`" = "" ]; then
                                        start_program
                                fi
                        else
                                echo "[error] $bin_dir/$program does not exist."
                        fi
                done
                sleep 10
        done
}

# watchdog start
watchdog_start(){
        $root_dir/$this_script watchdog &
        echo "watchdog started."
}

# watchdog stop
watchdog_stop(){
        pids=`getpids "$this_script watchdog"`
        for p in $pids
        do
                kill $p
        done
        echo "watchdog : Stopped"
}

# main function.
case "$1" in
        start)
                start
                watchdog_start
                ;;
        stop)
                watchdog_stop
                stop
                ;;
        status)
                status
                ;;
        restart)
                stop
                sleep 1
                start
                ;;
        watchdog)
                watchdog
                ;;
        *)
                echo $"Usage: $0 {start|stop|status|restart}"
                exit 1
                ;;
esac
