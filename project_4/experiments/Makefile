all: 	bbfs init mount_bbfs

init: 
	mkdir -p mountdir
	mkdir -p rootdir
	chmod 777 test_1.sh
	chmod 777 test_2.sh
	
clean_rootdir: 
	rm rootdir/*

clean_mountdir: 
	rm mountdir/*

bbfs: compile.sh
	chmod 777 compile.sh
	./compile.sh

mount_bbfs:
	../filesystem/bbfs rootdir/ mountdir/

unmount_bbfs:
	fusermount -u mountdir
	rm bbfs.log

clean: 
	rm -r mountdir/ rootdir/


