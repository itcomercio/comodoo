BINS="bzImage-romley.bin \
core-image-redoop-romley.tar.gz \
modules-*"

BUILD_HOST="BaldCompiler.cloud.cediant.es"

DEPLOY_PATH="/home/jroman/redoop-poky/build/tmp/deploy/images"

rm $BINS modules.tar.gz 2> /dev/null

for i in $BINS; do
	scp jroman@${BUILD_HOST}:${DEPLOY_PATH=}/$i .
done

mv modules* modules.tar.gz
