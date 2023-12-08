#include<stdio.h>
#include"syscall_wrapper.h"

int main(void){
	printf("Memory allocated: %lu\n", slob_get_total_alloc_mem_syscall_wrapper());

	printf("Free memory for a request: %lu\n", slob_get_total_free_mem_syscall_wrapper());

	return(0);
}

