#! /bin/sh

for k in prod uboot kernel; do
    openssl genpkey -algorithm RSA -out ${k}.key \
        -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537
done

openssl rsa -in prod.key -out prod.pub -pubout
#openssl rsa -in prod.pub -text -pubin -noout

rm -f dev.key

printf "\033[1;31m Please send back prod.key to Realtek for new DTE_FW_Certificate.bin!!!\n \033[m"
