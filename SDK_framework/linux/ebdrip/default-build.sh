#!/bin/sh
/mnt/hgfs/RipCore-4.05/SDK_framework/linux/ebdrip/build.sh release depend ebdrip -t linux-pentium -va pdls=all -va minimal=1 -va corefeatures=none -va blits=1 -va blits=2 -va blits=4 -va blits=8 -va blits=hwa1 -va blits=backdrop -va ICUbuiltin=1 -va cpp_compiler=gcc_4_5_3 -va ebd_pdfout=epdfouty -va rebuild=sw
