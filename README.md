# U-Boot for ZetiBoard

Forked from gnubee-git/GnuBee-MT7621-uboot

## How to build
Use docker image:
- Local build docker image: `docker build -f Dockerfile -t ghcr.io/zetifi/uboot-builder`
- Get docker image from github registry: `docker pull ghcr.io/zetifi/uboot-builder`

Build:
`docker run --rm -it -v .:/home/build --platform linux/386 ghcr.io/zetifi/uboot-builder /bin/bash -c ./build.sh`
