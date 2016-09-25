#!/bin/bash
#本脚本在debian5-debian9测试没有问题
if ! [ -x /usr/local/comp ]  ; then
wget http://mirrors.ustc.edu.cn/loongson/pmon/toolchain-pmon.tar.bz2 -c
tar jxvf toolchain-pmon.tar.bz2
mv usr/local/comp /usr/local/comp
rm -rf usr  
fi
PATH=/usr/local/comp/mips-elf/gcc-2.95.3/bin:$PATH
make pmontools
make config
make buildram
cp zloader/gzram .
scp gzram 192.168.12.7:/srv/tftp
#make build
#cp zloader/gzrom.bin .
#scp gzrom.bin 192.168.12.7:/srv/tftp

