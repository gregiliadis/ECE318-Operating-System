#Executing app 100 times.
#for i in {1..100}
#	do ./app 
#done > out_$1

mem_alloc=0
free_mem=0
compare="Memory allocated"

#Traversing through file and computing sum of
#allocated and free memory.
while IFS=:  read -r string value
do	
	if  [[ "$string" == "$compare" ]]
	then	
		#echo "$value"
		mem_alloc=$((mem_alloc+value))
	else 
		
		free_mem=$((free_mem+value))

	fi
done < out2

#Calculate average allocated and free memory.
mem_alloc=$((mem_alloc/100))
free_mem=$((free_mem/100))

echo "mem_alloc = $mem_alloc"
echo "free_mem = $free_mem"

