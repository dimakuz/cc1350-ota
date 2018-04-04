#!/bin/bash -xe

MAC=$1
BLOBS=${2:-ota_blobs}

cd $BLOBS
for FILE in $(ls -1 | sort)
do
	gatttool --device=$MAC \
		--char-write-req \
		--handle=0x24 \
		--value=$(cat $FILE)
done
