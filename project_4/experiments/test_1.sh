#A shell script for some checks in hw4


######################################
############# FUNCTIONS ##############
#Calculate storage's space
calculate_storage(){
	BLOCK_SIZE=512
	METADATA=32
	BLOCK_DIFF=1
	LAST_BYTE_DIFF=-1
	
	echo "$1 $2 $3 $4"
	FILESIZE1=$(stat -c%s "$1")
	FILESIZE2=$(stat -c%s "$2")

	FILESIZED1=$(stat -c%s "$3")
	FILESIZED2=$(stat -c%s "$4")

	echo "Check the file sizes"

	echo "Size of $FILE1 is $FILESIZE1"
	echo "Size of $FILED1 is $FILESIZED1"
 
	if [ $FILESIZE1 = $FILESIZED1 ]
	then 
		echo "Test size of $1 is equal to $3: SUCCESS"
	else
		echo "Test size of $1 is equal to $3: FAILURE"
	fi
	
	echo "Size of $FILE2 is $FILESIZE2"
	echo "Size of $FILED2 is $FILESIZED2"
 

	if [ $FILESIZE2 = $FILESIZED2 ]
	then 
		echo "Test size of $2 is equal to $4: SUCCESS"
	else
		echo "Test size of $2 is equal to $4: FAILURE"
	fi


	echo "Bytes different (byte_offset, value, ASCII, value, ASCII)" 	
	cmp -lb $3 $4
	cmp -l $3 $4 > tmp

	while IFS=" " read -r F1 F2 F3
	do 
		if [ $LAST_BYTE_DIFF = -1 ]
		then
			LAST_BYTE_DIFF=$F1
		else
			BLOCK_PREVIOUS_BYTE=$((F1/BLOCK_SIZE))
			BLOCK_CURRENT_BYTE=$((LAST_BYTE_DIFF/BLOCK_SIZE))
			if [ $BLOCK_PREVIOUS_BYTE != $BLOCK_CURRENT_BYTE ]	
			then 
				BLOCK_DIFF=$((BLOCK_DIFF+1))
			fi
			LAST_BYTE_DIFF=$F1
		fi			
	done < tmp

	rm tmp
	if [ $BLOCK_DIFF = 1 ]
	then
		echo "There are 0 blocks different between $FILED1 $FILED2"
		BLOCK_DIFF=0
	else
		echo "There are $BLOCK_DIFF blocks different between $FILED1 $FILED2"
	fi

	if [ $FILESIZED2 -gt $FILESIZED1 ]
	then 	
		FILESIZE_MIN=$FILESIZE1
	else	
		FILESIZE_MIN=$FILESIZE2
	fi

	if [[ $((FILESIZE_MIN % 2)) -eq 0 ]]
	then 
		BLOCKS_MIN_FILE=$((FILESIZE_MIN/512))
	else
		BLOCKS_MIN_FILE=$(((FILESIZE_MIN/512)+1))
	fi

	EXP_STORAGE_SIZE=$(((BLOCKS_MIN_FILE+BLOCK_DIFF)*(BLOCK_SIZE+METADATA)))

	echo "EXP_STORAGE_SIZE = (BLOCKS_MIN_FILE+BLOCK_DIFF) * (BLOCK_SIZE+METADATA)" 
	echo "EXP_STORAGE_SIZE = ($BLOCKS_MIN_FILE+$BLOCK_DIFF) * ($BLOCK_SIZE+$METADATA)"
	echo "EXP_STORAGE_SIZE = $EXP_STORAGE_SIZE"
	echo "Expected storage size is $EXP_STORAGE_SIZE"

	STORAGE="rootdir/.storage"
	STORAGESIZE=$(stat -c%s "$STORAGE")
	echo "Storage size via ls -la"
	ls -la $STORAGE

	if [ $STORAGESIZE != $EXP_STORAGE_SIZE ]
	then 
		echo "Test for storage size: FAILED"
	else
		echo "Test for storage size: SUCCESS"
	fi
}
##################################################
########### SHELL'S MAIN CODE BEGIN ##############
echo "###### TEST BEGINS ######"
FILE1="test_files/test_8kb.txt"
FILE2="test_files/test_8kb_1.txt"
FILED1="mountdir/test_8kb.txt"
FILED2="mountdir/test_8kb_1.txt"
FILEC1="rootdir/test_8kb.txt"
FILEC2="rootdir/test_8kb_1.txt"

CP1=$(cp $FILE1 $FILED1)
CP2=$(cp $FILE2 $FILED2)

DIFF1=$(diff $FILE1 $FILED1)
DIFF2=$(diff $FILE1 $FILED1)

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

echo "Contents of mountdir (via ls -la)"
ls -la mountdir/
echo "Contents of rootdir (via ls -la)"
ls -la rootdir/
echo "Contents of test_files (via ls -la)"
ls -la test_files/

echo "Open $FILED1 and $FILEC1?[Y/N]"
read ans
if [ $ans = Y ]
then
	gedit $FILED1
	gedit $FILEC1 
fi
echo
echo "Open $FILED2 and $FILEC2?[Y/N]"
read ans
if [ $ans = Y ]
then
	gedit $FILED2 
	gedit $FILEC2 > tmp
	rm tmp
fi
echo
echo "Press a button to continue the test."
read ans

echo "##############################################################"
echo "TEST FOR STORAGE"

calculate_storage $FILE1 $FILE2 $FILED1 $FILED2
echo "##############################################################"
cp $FILE1 $FILE1~
cp $FILE1 $FILE1~
#echo "Truncate check and size not an integer multiple of 2KB:"
#echo "Decrease size to 4,5kb (4608bytes):"

#echo "Truncate: truncate -s 4608 $FILED1"
#echo "Truncate: truncate -s 4608 $FILED2"
truncate -s 4608 $FILED1	
truncate -s 4608 $FILED2

truncate -s 4608 $FILE1~	
truncate -s 4608 $FILE2~
DIFF3=$(diff $FILE1~ $FILED1)
DIFF4=$(diff $FILE1~ $FILED1)

if [[ "$DIFF3" != "" && "$DIFF4" != "" ]]
then 	
	echo "Truncate diff test: FAILED"
else
	echo "Truncate diff test: SUCCESS"
fi
calculate_storage $FILE1~ $FILE2~ $FILED1 $FILED2 
echo "##############################################################"
echo "Truncate check:"
echo "Decrease size to 0:"

echo "Truncate: truncate -s 0 $FILED1"
echo "Truncate: truncate -s 0 $FILED2"
truncate -s 0 $FILED1	
truncate -s 0 $FILED2

truncate -s 0 $FILE1~	
truncate -s 0 $FILE2~

calculate_storage  $FILED1 $FILED2 $FILE1~ $FILE2~ 
rm $FILE1~ $FILE2~
echo "####### TEST FINISHED #######"
