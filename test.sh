#!/usr/bin/env bash


#Menu usage
while true; do


clear
echo "====Labo 1===="
echo "1. Testbench"
echo "2. Installe le pilote"
echo "3. Desinstalle le pilote"

read -n1 choix

case $choix in
  1) sudo /home/jf/ELE784/lab1/cmake-build-debug/LAB1
  ;;

  2) echo " Installation du pilote"
    make clean
    make
    sudo insmod ./buf.ko
    sudo chmod 666 /dev/Buf_node
  ;;
  3) echo " Desinstallation du pilote"
        sudo rmmod ./buf.ko
        echo "Pilote desinstaller"
  ;;

  f)   sudo rmmod -f ./buf.ko
        echo "Pilote desinstaller by force"
       ;;
  *) echo " choix invalide";;
esac
sleep 1s

done
# make -C /usr/src/linux-source-3.16.0 M=`pwd` modules
# sudo insmod ./HelloDev.ko
# sudo chmod 666 /dev/HelloDev_Node
# sudo rmmod HelloDev
# lsmod | grep HelloDev
# ls -la /dev | grep HelloDev
# cat /proc/devices
# ls -la /sys/module/Hello*
# ls -la /sys/devices/virtual | grep Hello
# ls -la /sys/devices/virtual/HelloDevClass
# ls -la /sys/devices/virtual/HelloDevClass/HelloDev_Node
#
# dmesg | tail -20
# watch -n 0,1 "dmesg | grep HelloDev | tail -n $((LINES-6))"
#rmmod