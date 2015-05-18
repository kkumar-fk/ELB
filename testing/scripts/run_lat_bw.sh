#!/bin/bash

function my_add
{
        s="0.0"
        while [ $# -gt 0 ]
        do
                s=$s+"$1"
                shift 1
        done
        echo $s | bc -l
}

SERVER=172.20.232.106
PORT=80
HOSTS="172.20.232.112 172.20.232.114 172.20.232.117 172.20.232.119"
NREQUESTS=1000000
NCONCURRENT=500
SIZE=64

echo "$0: Server Port #Requests #Concurrent #I/O-Size"

if [ $# -gt 0 ]
then
	SERVER=$1
	if [ $# -gt 1 ]
	then
		PORT=$2
		if [ $# -gt 2 ]
		then
			NREQUESTS=$3
			if [ $# -gt 3 ]
			then
				NCONCURRENT=$4
				if [ $# -gt 4 ]
				then
					SIZE=$5
				fi
			fi
		fi
	fi
fi

dir=RESULTS/$$/
mkdir -p $dir

# One time - create ssh keys and copy to the test systems
# ssh-keygen 
# if [ $? -eq 0 ]
# then
	# for host in $HOSTS
	# do
		# ssh-copy-id $host
	# done
# fi

echo
echo "Testing against $HOSTS"
echo "Command: ab -k -n $NREQUESTS -c $NCONCURRENT http://$SERVER:$PORT/$SIZE"
echo

for host in $HOSTS
do
	ssh "$host" "ab -k -n $NREQUESTS " \
		"-c $NCONCURRENT http://$SERVER:$PORT/$SIZE \
		> /tmp/ab-result 2>/dev/null" &
done
wait

for host in $HOSTS
do
	ssh $host "cat /tmp/ab-result" > $dir/output.`echo $host | cut -d. -f4`
done

list=`egrep -h "Requests per second" $dir/* | awk '{print $4}' | tr '\n' '+' | sed 's/+$//'`
echo -n "RPS: $list = "
sum=$(my_add `egrep -h "Requests per second" $dir/* | awk '{print $4}'`)
echo "$sum #/s"

list=`egrep -h "Transfer rate" $dir/* | awk '{print $3}' | tr '\n' '+' | sed 's/+$//'`
echo -n "BW:  $list = "
sum=$(my_add `egrep -h "Transfer rate" $dir/* | awk '{print $3}'`)
summ=`echo "scale=2; ($sum*8/1024)" | bc -l`
sumg=`echo "scale=2; ($summ/1024)" | bc -l`

echo "$sum Kb/s"
echo "		($summ Mb/s, or $sumg Gb/s)"
if [ $(echo " $sumg >= 10" | bc) -eq 1 ]
then 
	echo "Tests unsynchronized - BW >= 10gbps (assuming 10G card)"
fi

echo
