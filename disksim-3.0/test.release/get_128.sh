#!/bin/bash
in_filename=$1
out_filename=$2
cat ${in_filename}|var=$5|echo ${var:0:128} >>128.txt
