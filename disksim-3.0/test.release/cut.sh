#!/bin/bash
head -n 10000 $1 > $2
tail -n 10000 $1 >> $2
