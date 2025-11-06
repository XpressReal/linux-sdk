#! /bin/sh

#openssl enc -nosalt -aes-256-cbc -k secret -md sha256 -P > aeskey.key
head -c 32 /dev/urandom > prodkey.bin
head -c 32 /dev/urandom > ubootkey.bin
head -c 32 /dev/urandom > kernelkey.bin
head -c 32 /dev/urandom > aesiv.bin

rm -f devkey.bin

printf "\033[1;31m Please send back prodkey.bin to Realtek for new DTE_FW_Certificate.bin!!!\n \033[m"
