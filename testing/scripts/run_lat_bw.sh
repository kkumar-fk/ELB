#!/bin/bash

function add
{
	s="0.0"
	while [ $# -gt 0 ]
	do
		s=$s+$1
		shift 1
	done
	echo $s

	echo $s | bc -l
}

add 2.1 3.2 4.4 5.1

kkumar@kkumar-Latitude-E5440:~$ ./x
0.0+2.1+3.2+4.4+5.1
14.8
kkumar@kkumar-Latitude-E5440:~$ s1
spawn ssh -X root@172.20.232.66
root@172.20.232.66's password: 
X11 forwarding request failed on channel 0
Linux 172-20-232-66 3.19.6-KK #3 SMP Fri May 1 00:36:33 IST 2015 x86_64


Plan your installation, and FAI installs your plan.

Last login: Tue May 19 04:42:32 2015 from 172.20.228.15
root@172-20-232-66:~# vi run_test 
root@172-20-232-66:~# 
root@172-20-232-66:~# cat run_test 
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
REQUESTS=1000000
CONCURRENT=500
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
			REQUESTS=$3
			if [ $# -gt 3 ]
			then
				CONCURRENT=$4
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

#  ssh-keygen && for host in $(cat hosts.txt); do ssh-copy-id $host; done

echo
echo "Testing against $HOSTS"
echo "Command: ab -k -n $REQUESTS -c $CONCURRENT http://$SERVER:$PORT/$SIZE"
echo

#for host in $(cat hosts.txt)
for host in $HOSTS
do
	ssh "$host" "ab -k -n $REQUESTS " \
		"-c $CONCURRENT http://$SERVER:$PORT/$SIZE > /tmp/ab-result 2>/dev/null" &
done
wait

for host in $HOSTS
do
	ssh $host "cat /tmp/ab-result" > $dir/output.`echo $host | cut -d. -f4`
done

list=`egrep -h "Requests per second" $dir/* | awk '{print $4}' | tr '\n' ' '`
echo -n "RPS: $list = "
v=$(my_add `egrep -h "Requests per second" $dir/* | awk '{print $4}'`)
echo "$v #/sec"

list=`egrep -h "Transfer rate" $dir/* | awk '{print $3}' | tr '\n' ' '`
echo -n "Bandwidth: $list = "
v=$(my_add `egrep -h "Transfer rate" $dir/* | awk '{print $3}'`)
echo "$v Kb/sec"
root@172-20-232-66:~# !v
vi run_test 
root@172-20-232-66:~# !v
vi run_test 
root@172-20-232-66:~# bc -l
bc 1.06.95
Copyright 1991-1994, 1997, 1998, 2000, 2004, 2006 Free Software Foundation, Inc.
This is free software with ABSOLUTELY NO WARRANTY.
For details type `warranty'. 
615550.26+407437.63+287251.82+299665.05
1609904.76
./1024/1024*8
12.28259857177734375000
root@172-20-232-66:~# cat run_test 
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
