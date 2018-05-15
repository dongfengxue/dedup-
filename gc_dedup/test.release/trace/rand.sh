#!/bin/bash
line=10000
#line=300
for ((i=0;i<${line};i++))
do
	#int x=rand(10)	
	echo $(($RANDOM%7+1)) >> rand.trace
done
