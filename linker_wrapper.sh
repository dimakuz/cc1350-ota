#!/bin/bash
OUTFILE=$1
shift

env | tee /tmp/l

OBJCOPY="$(dirname $1)/../../gcc-arm-none-eabi-6-2017-q1-update/bin/arm-none-eabi-objcopy"
OBJDUMP="$(dirname $1)/../../gcc-arm-none-eabi-6-2017-q1-update/bin/arm-none-eabi-objdump"

for OTA_OBJ in ota_app/*.obj;
do
	echo Processing OTA file: $OTA_OBJ
	for SECTION in $($OBJDUMP -h $OTA_OBJ  | egrep -v '\.ota\.' | egrep '\.(text|bss|data|const|cinit)' | awk '{ print $2 }');
	do
		$OBJCOPY \
			--rename-section $SECTION=.ota$SECTION \
			$OTA_OBJ
	done
done

echo OUTFILE: $OUTFILE | tee -a /tmp/l
echo $@ | tee -a /tmp/l
$@

echo Working Directory: $(pwd) | tee -a /tmp/l
echo Running python ../extract_ota.py $OUTFILE ota_app/*.obj \> ota.json | tee -a /tmp/l
echo $OBJCOPY --dump-section .ota.text=output.bin $OUTFILE | tee -a /tmp/l
python ../extract_ota.py $OUTFILE ota_app/*.obj > ota.json
