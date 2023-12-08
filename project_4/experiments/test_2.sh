#A shell script for some checks in hw4
##################################################
########### SHELL'S MAIN CODE BEGIN ##############
echo "###### TEST BEGINS ######"
FILE1="test_files/test_8kb.txt"
FILE2="test_files/test_8kb_1.txt"
FILED1="mountdir/test_8kb.txt"
FILED2="mountdir/test_8kb_1.txt"
FILEC1="rootdir/test_8kb.txt"
FILEC2="rootdir/test_8kb_1.txt"

MOUNT=$(mount | grep "bbfs")
if [ "$MOUNT" != "" ]
then 
	echo "BBFS is mounted on mountdir"
	echo "Removing mountdir's contents"
else
	echo "BBFS is not mounted on mountdir"
	echo "Mount BBFS on mountdir"
	../filesystem/bbfs rootdir mountdir 
fi

rm mountdir/*

CP1=$(cp $FILE1 $FILED1)
CP2=$(cp $FILE2 $FILED2)


if [[ "$CP1" = "" || "$DIFF1" = "" ]]
then 
	echo "Copying $FILE1 to $FILED1: SUCCESS"
else
	echo "Copying $FILE1 to $FILED1: FAILED"
fi

if [[ "$CP2" = "" || "$DIFF2" = "" ]]
then 
	echo "Copying $FILE2 to $FILED2: SUCCESS"
else
	echo "Copying $FILE2 to $FILED2: FAILED"
fi

echo "Press a button to unmount bbfs from mountdir:"
read -n2 ans

echo "Unmount mountdir: fusermount -u mountdir"
fusermount -u mountdir

echo "Press a button to mount bbfs to mountdir:"
read -n2 ans

../filesystem/bbfs rootdir mountdir 

echo "BBFS mounted"

ls -l mountdir/

DIFF3=$(diff $FILE1 $FILED1)
DIFF4=$(diff $FILE2 $FILED2)
if [[ "$DIFF3" = "" && "$DIFF4" = "" ]]
then 
	echo "NON Volatile: SUCCESS"
else
	echo "NON Volatile: FAILED"
fi
