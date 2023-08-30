#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(hello)
{
	printk("Hello world\n");
	printk("310512014\n");
	return 0;
}
