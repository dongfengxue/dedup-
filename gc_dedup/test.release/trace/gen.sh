#!/bin/bash
in_filename=$1
out_filename=$2
head -10000 ${in_filename} | awk '{print $1 "\t " $2 " \t" $3 " \t" $4" \t" $5 " \t"$6"\t "$7 }'>> ${out_filename}
