#! /bin/bash
if [ -z "$1" ]
  then
    echo
    echo "Copies all rosco_m68k Xosera tests (like to an SD card)".
    echo "       copy_m68k_tests.sh <target_path>"
    echo "E.g.:  copy_m68k_tests.sh /Volumes/ROSCO_SD"
    echo
    echo "NOTE: You must make sure the media is mounted before running this script"
    echo "      and also make sure to unmount media safely after files are copied!"
    exit
fi
echo "=== Copying rosco_m68k Xosera tests -> $1"
echo
cp -v xosera_ansiterm_m68k/xosera_ansiterm_m68k.bin "$1"
cp -v xosera_vramtest_m68k/xosera_vramtest_m68k.bin "$1"
cp -v xosera_test_m68k/xosera_test_m68k.bin "$1"
cp -v copper/splitscreen_test_m68k/splitscreen_test_m68k.bin "$1"
cp -v copper/copper_test_m68k/copper_test_m68k.bin "$1"
cp -v copper/crop_test_m68k/crop_test_m68k.bin "$1"
cp -v testdata/raw/*.raw "$1"
