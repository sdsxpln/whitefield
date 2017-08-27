#!/bin/bash

[[ ! -f "config.inc" ]] && echo "Need to start whitefield from base folder!!" && exit 1
. config.inc

export LD_LIBRARY_PATH=$AIRLINE_NS3/build:$BINDIR
export FORKER=$BINDIR/wf_forker
export LOGPATH=log
export MONITOR_PORT=$MONITOR_PORT

if [ "$1" == "gdb" ]; then
	shift
	cmdprefix="gdb --args"
fi

[[ ! -f "$1" ]] && echo "Usage: $0 <wf_config_file>" && exit

mkdir $LOGPATH pcap 2>/dev/null
$cmdprefix $BINDIR/whitefield $* &
sleep 1
echo ;
