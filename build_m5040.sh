#!/bin/bash
#本脚本在debian5-debian9测试没有问题
if ! [ -x /usr/local/comp ]  ; then
wget http://mirrors.ustc.edu.cn/loongson/pmon/toolchain-pmon.tar.bz2 -c
tar jxvf toolchain-pmon.tar.bz2
mv usr/local/comp /usr/local/comp
rm -rf usr  
fi
PATH=/usr/local/comp/mips-elf/gcc-2.95.3/bin:$PATH
make config
cd zloader.2ftopstar
make cfg
make all tgt=ram
cp gzram ../gzram.2ftopstar
make all tgt=rom
cp gzrom.bin ../pmon_2ftopstar.bin
