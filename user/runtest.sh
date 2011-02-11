#!/bin/bash
pid=`echo $$`
dtrace=./build-*/dtrace
if [ ! -f $dtrace ]; then
	echo "No dtrace command line";
	exit 1;
fi

for i in $(ls ./demo/*/*.d)
do
cmd=$dtrace\ -e\ -s\ $i
	if [ $i == "./demo/buf/ring.d" ] ||
	   [ $i == "./demo/sched/whererun.d" ]; then
		cmd=$cmd\ /bin/date
	elif [ $i == "./demo/user/badopen.d" ] ||
	     [ $i == "./demo/struct/rwinfo.d" ] ||
	     [ $i == "./demo/script/tracewrite.d" ] ||
	     [ $i == "./demo/profile/profpri.d" ] ||
	     [ $i == "./demo/intro/rwtime.d" ] ||
	     [ $i == "./demo/intro/trussrw.d" ]; then
		cmd=$cmd\ $pid
	elif [ $i == "./demo/user/errorpath.d" ]; then
		cmd=$cmd\ $pid\ _chdir
	elif [ $i == "./demo/sched/pritime.d" ]; then
		cmd=$cmd\ $pid\ 1
	fi
$cmd
echo $cmd
done
