#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

SYSCALL_DEFINE2(revstr, int, len_count, char __user *, src)
{
	char buf[256];
	unsigned long chunklen = sizeof(buf);
	int i;
	char temp;
	unsigned long len_max = len_count;
	if(len_max < chunklen) chunklen = len_max+1;
	if(copy_from_user(buf, src, chunklen)){
		return -EFAULT;
	}
	printk("The origin string:%s\n",buf);

	for(i = 0;i < len_count/2;i++){
		temp = buf[i];
		buf[i] = buf[len_count-i-1];
		buf[len_count-i-1] = temp;
	}
	printk("The reversed string:%s\n",buf);
	return 0;
}
