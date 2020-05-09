# if need
sudo rmmod driver.ko
# build driver
make
# add him to drivers
sudo insmod driver.ko
# if need (change major)
sudo rm /dev/miniFSdriver
# see `dmesg | grep /dev/miniFSdriver` for correct command
sudo mknod /dev/miniFSdriver c 240 0
