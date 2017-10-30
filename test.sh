#!/usr/bin/env bash


#Menu usage
while true; do


clear
echo "====Labo 1===="
echo "1. Ouvrir en Ecriture"
echo "2. Ouvrir en Lecture"
echo "3. Installe le pilote"
echo "4. Desinstalle le pilote"
echo "5. Monitoring du pilote"

read -n1 choix

case $choix in
  1) echo " Ouverture du pilote en ecriture"
     sudo /home/jf/ELE784/lab1/cmake-build-debug/LAB1
  ;;
  2) echo " Ouverture du pilote en lecture"
     echo "Lecture du pilote en entier"
     cat /dev/Buf_node
   ;;
  3) echo " Installation du pilote"
    make clean
    make
    sudo insmod ./buf.ko
    sudo chmod 666 /dev/Buf_node

    exit
  ;;
  4) echo " Desinstallation du pilote"
        sudo rmmod ./buf.ko
        echo "Pilote desinstaller"
  ;;
  5)    clear
        dmesg | tail -20
        ;;

  f)   sudo rmmod -f ./buf.ko
        echo "Pilote desinstaller by force"
       ;;
  *) echo " choix invalide";;
esac
sleep 2s

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