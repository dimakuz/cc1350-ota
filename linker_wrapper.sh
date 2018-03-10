#!/bin/bash

env | tee /tmp/l

OBJCOPY="$(dirname $1)/../../gcc-arm-none-eabi-6-2017-q1-update/bin/arm-none-eabi-objcopy"
OBJDUMP="$(dirname $1)/../../gcc-arm-none-eabi-6-2017-q1-update/bin/arm-none-eabi-objdump"

for OTA_OBJ in ota_app/*.obj;
do
	echo Processing OTA file: $OTA_OBJ
	for SECTION in $($OBJDUMP -h $OTA_OBJ  | egrep -v '\.ota\.' | egrep '\.(text|bss|data)' | awk '{ print $2 }');
	do
		$OBJCOPY \
			--rename-section $SECTION=.ota$SECTION \
			$OTA_OBJ
	done
done

echo $@ | tee -a /tmp/l
$@
