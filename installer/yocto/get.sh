BINS="bzImage-romley.bin \
core-image-comodoo-romley.tar.gz \
modules-*"

BUILD_HOST="BaldCompiler.cloud.cediant.es"

DEPLOY_PATH="/home/jroman/comodoo-poky/build/tmp/deploy/images"

rm $BINS modules.tar.gz 2> /dev/null

for i in $BINS; do
	scp jroman@${BUILD_HOST}:${DEPLOY_PATH=}/$i .
done

mv modules* modules.tar.gz
