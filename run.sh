#!/bin/sh
rm -rf *.o *.exe *.a *.so
gcc camforder.c log.c noly.c -o camforder.exe

cvr_server_ip="54.83.20.84"
cvr_server_port="8080"

ipcam_server_ip="10.5.150.5"
ipcam_server_port="554"
ipcam_rtsp_path="channel1"

device_mac="f835dd000001"
device_id="600000123"

echo ""
echo "---------------------------------------------------------------------"
echo "VLC Play: rtsp://$cvr_server_ip:80/v1/live?device_id=$device_id"
echo "---------------------------------------------------------------------"
echo ""

./camforder.exe -s $cvr_server_ip \
                -p $cvr_server_port \
                -h $ipcam_server_ip \
                -o $ipcam_server_port \
                -l $ipcam_rtsp_path \
                -d $device_mac \
                -u $device_id
