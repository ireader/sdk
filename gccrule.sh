#!/bin/sh

OUTPATH=$1
SOURCE_FILES=$2
CC=$3
CXX=$4

for item in $SOURCE_FILES; do
	srcfile=$(basename $item)
	objfile=$OUTPATH/${srcfile%.*}.o
	depfile=${objfile%.*}.d
	
	if [ $item -nt $depfile ]; then
		if [ "${item##*.}" = "c" ]; then
			rule=$($CC -MM $item)
			deprule=${rule##*:}
			echo "$objfile:$deprule" > $depfile
			echo "$objfile: $item" >> $depfile
			echo '	$(COMPILE.CC) -c -o $@ $<' >> $depfile
		else
			rule=$($CXX -MM $item)
			deprule=${rule##*:}
			echo "$objfile:$deprule" > $depfile
			echo "$objfile: $item" >> $depfile
			echo '	$(COMPILE.CXX) -c -o $@ $<' >> $depfile
		fi
	fi
done
