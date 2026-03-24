#!/bin/sh
# Generate SSL certs for kvmd (VNC) and nginx

generate_cert() {
    local CN="$1"
    local DIR="$2"
    local KEY_PATH="$DIR/server.key"
    local CRT_PATH="$DIR/server.crt"
    local BITS=2048

    if [[ ! -f "$KEY_PATH" || ! -f "$CRT_PATH" ]]; then
        echo "Generating SSL certificate for CN=$CN ..."
        mkdir -p "$DIR"

        openssl req -x509 -nodes -days 365 \
            -newkey rsa:$BITS \
            -keyout "$KEY_PATH" \
            -out "$CRT_PATH" \
            -subj "/CN=$CN"

        chmod 600 "$KEY_PATH"
        chmod 644 "$CRT_PATH"
        echo "Certificate for $CN created at $DIR"
    else
        echo "Certificate for $CN already exists at $DIR — skipping."
    fi
}

# KVMD (VNC)
generate_cert "kvmd.local" "/etc/kvmd/vnc/ssl"

# Nginx
generate_cert "kvmd.local" "/etc/kvmd/nginx/ssl"
# Generate Nginx Configuration files
/usr/bin/kvmd-nginx-mkconf /etc/kvmd/nginx/nginx.conf.mako /run/kvmd/nginx.conf
