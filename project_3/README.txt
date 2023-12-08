*****************************************************
**************ECE-318 OPERATING SYSTEMS**************
*****************************************************
*****************************************************


		       HW3


AUTHORS: GRIGORIS ILIADIS        2522 giliadis@uth.gr
	 DIMITRIS KEFALAS        2533 dkefalas@uth.gr
	 FOIVOS   POURNAROPOULOS 2614 spournar@uth.gr

*Code that implements slob allocator: /usr/src/linux-3.14.62-dev/mm/slob.c
IN ORDER TO USE BEST FIT ALLOCATOR BE SURE THAT __BEST_FIT is defined.

*In order to define BEST_FIT use this command: #define __BEST_FIT

*In order to check the iteration of the slob list from the page pointed
 from limit by printing the pages each 6000 calls of slob_alloc
 use the command: #define __BEST_FIT_DEBUG_ON

*The implementation of system cals is in /usr/src/linux-3.14.62-dev/kernel/hw3_syscalls.c

*The prototypes of functions that are used in order to get access to total allocated memory
 and total free memory are declared in /usr/src/linux-3.14.62-dev/include/linux/mm.h.

*User level application: 
 Compile:
	Use the makefile.
	command: make

 A script that runs the application for 100 times and calculates the average of results
 is implemented.

 Results correspond to total free memory and to total allocated memory.
 Run script: chmod 777 script.sh
             ./script.sh name_of_file_used_for_results_storage
 
FOR MORE DETAILS CHECK THE COMMENTS IN EACH SOURCE CODE.
