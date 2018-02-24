BRPATH=/home/wzab/biezace/PW/ZBIO_revival/Trenz_pliki/Server_JTAG/buildroot-2018.02-rc2
(
export PATH=$BRPATH/output/host/usr/bin:$PATH
make ARCH=arm CROSS_COMPILE=\
arm-linux-gnueabihf- xvcd-ft2232h
)

