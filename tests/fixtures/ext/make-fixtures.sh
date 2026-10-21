#!/bin/bash
# Rebuilds the signed test APKs in this folder. They are tiny zips, not
# extensions: the tests only read who signed them. Needs the JDK (keytool,
# jarsigner) and Android build-tools (apksigner); the outputs are committed,
# so tests do not need either.
set -euo pipefail
cd "$(dirname "$0")"
tools=${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}/build-tools
apksigner=$(ls -d "$tools"/*/apksigner | sort -V | tail -1)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for name in alpha beta; do
    keytool -genkeypair -keystore "$work/$name.jks" -storepass testpass -keypass testpass -alias "$name" \
        -keyalg RSA -keysize 2048 -validity 36500 -dname "CN=SCANCORE test $name" >/dev/null 2>&1
done

mkdir -p "$work/content/assets"
echo "SCANCORE test package" > "$work/content/assets/readme.txt"
(cd "$work/content" && zip -q -X -r "$work/unsigned.apk" .)

# v1 only (JAR signature), with alpha.
cp "$work/unsigned.apk" "$work/v1.apk"
jarsigner -keystore "$work/alpha.jks" -storepass testpass -sigalg SHA256withRSA -digestalg SHA-256 \
    "$work/v1.apk" alpha >/dev/null
cp "$work/v1.apk" tiny-v1.apk

# v1 + v2 + v3, with alpha and with beta.
for name in alpha beta; do
    "$apksigner" sign --ks "$work/$name.jks" --ks-pass pass:testpass --min-sdk-version 21 \
        --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true \
        --out "tiny-$name.apk" "$work/unsigned.apk"
    rm -f "tiny-$name.apk.idsig"
    keytool -exportcert -keystore "$work/$name.jks" -storepass testpass -alias "$name" -rfc 2>/dev/null \
        | openssl x509 -outform der | sha256sum | cut -d' ' -f1 > "key-$name.txt"
done
echo "alpha $(cat key-alpha.txt)"
echo "beta  $(cat key-beta.txt)"
