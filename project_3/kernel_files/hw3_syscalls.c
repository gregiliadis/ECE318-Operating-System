#include<linux/syscalls.h>
#include<linux/kernel.h>
#include<linux/mm.h>

SYSCALL_DEFINE0( slob_get_total_alloc_mem ) {
	return(get_allocated_mem());
}

SYSCALL_DEFINE0( slob_get_total_free_mem ) {
	return(get_free_mem());
}
