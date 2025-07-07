#!/bin/bash

# Adapted from https://arminreiter.com/2022/01/create-your-own-certificate-authority-ca-using-openssl/

# generate aes encrypted private key
openssl ecparam -genkey -name prime256v1 -out ca.key

# create certificate
openssl req -x509 -new -nodes -key ca.key -sha256 -days 36500 -out ca.crt -subj '/CN=My Root CA'

# create certificate for service
openssl ecparam -genkey -name prime256v1 -out server.key
openssl req -new -sha256 -key server.key -noenc -out server.csr -subj "/CN=localhost"

# create a v3 ext file for SAN properties
cat > server.v3.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
IP.1 = 127.0.0.1
EOF

openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 36500 -sha256 -extfile server.v3.ext

# deno run --allow-all 'data:,Deno.serve({ port: 8001, cert: Deno.readTextFileSync("server.crt"), key: Deno.readTextFileSync("server.key") }, () => new Response("hello"))'
