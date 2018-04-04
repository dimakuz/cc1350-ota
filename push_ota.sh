#!/bin/bash -xe

MAC=$1

cd ./ota_blobs/
for FILE in $(ls -1 | sort)
do
	gatttool --device=$MAC \
		--char-write-req \
		--handle=0x24 \
		--value=$(cat $FILE)
done
