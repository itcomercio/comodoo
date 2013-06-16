BINS="beetlepos-image-beetlepos.tar.gz bzImage-beetlepos.bin modules-*"

for i in $BINS; do
	scp builder@ameba:/builds/ASV1-BEETLEPOS/build/tmp/deploy/images/beetlepos/$i .
done
