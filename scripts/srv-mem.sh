#!/bin/bash

# Print out drawpile-srv's memory usage.
# Output consists of timestamp + process number and the following columns:
# - Maximum resident set size (waterline)
# - Current resident set size
# - Swapped out virtual memory size

PIDS=$(pgrep drawpile-srv)

if [ -z "$PIDS" ]
then
	exit
fi

NOW=$(date -Iseconds)

for PID in $PIDS
do
	echo -n "$NOW [$PID]; "
	egrep "^Vm(HWM|RSS|Swap)" /proc/$PID/status | awk '{ printf "%s %s; ", $2, $3 }'
	echo
done

