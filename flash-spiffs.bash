set -x

$IDF_PATH/components/spiffs/spiffsgen.py 1536000 spiffs/ spiffs.bin

address=$(python $IDF_PATH/components/partition_table/gen_esp32part.py build/partitions.bin |
	awk -F "," '/spiffs/ { print $4; }')

$IDF_PATH/components/esptool_py/esptool/esptool.py --baud 921600 write_flash $address spiffs.bin
