#!/bin/sh
IMAGE=ianburgwin/firmbuilder

set -eux
docker run -it --rm -v $PWD:/host $IMAGE make $@
