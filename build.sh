#! /bin/bash

cp MT7621A_defconfig .config
make -j$(nproc)