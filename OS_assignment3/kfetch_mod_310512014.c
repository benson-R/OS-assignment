/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/printk.h> /* for pr_* */
#include <linux/cpumask.h> /* for num_online_cpus */
#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/sysinfo.h>
#include <asm/processor.h>
#include <linux/seq_file.h>

static int kfetch_init(void);
static void kfetch_cleanup(void);
static int kfetch_open(struct inode *, struct file *);
static int kfetch_release(struct inode *, struct file *);
static ssize_t kfetch_read(struct file *, char *, size_t, loff_t *);
static ssize_t kfetch_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "kfetch"   /* Dev name as it appears in /proc/devices   */
#define BUF_LEN 1024              /* Max length of the message from the device */
#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE   (1 << 0)/* Kfetch infomation mask */
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)
/*
 * Global variables are declared as static, so are global within the file.
 */

static int Major;               /* Major number assigned to our device driver */
static int kfetch_Open = 0;     /* Is device open?
                                 * Used to prevent multiple access to device */
static char kfetch_buf[BUF_LEN];       /* The msg the device will give when asked */

static struct class *cls;

struct kfetch_info {
    char *release;
    char *num_cpus;
    char *cpu_model;
    char *mem;
    char *uptime;
    char *num_procs;
};

struct kfetch_mask{
    int mask;
};

int len_read;
struct kfetch_info info;
struct kfetch_mask info_mask;

static struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read = kfetch_read,
    .write = kfetch_write,
    .open = kfetch_open,
    .release = kfetch_release
};

/*
 * This function is called when the module is loaded
 */
static int kfetch_init(void)
{
    Major = register_chrdev(0, DEVICE_NAME, &kfetch_ops);

    if (Major < 0) {
        pr_alert("Registering char device failed with %d\n", Major);
        return Major;
    }

    pr_info("I was assigned major number %d.\n", Major);

    cls = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(cls, NULL, MKDEV(Major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);
    

    return 0;
}

/*
 * This function is called when the module is unloaded
 */
static void kfetch_cleanup(void)
{
    device_destroy(cls, MKDEV(Major, 0));
    class_destroy(cls);

    /*
     * Unregister the device
     */
    unregister_chrdev(Major, DEVICE_NAME);
}

/*
 * Methods
 */

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int kfetch_open(struct inode *inode, struct file *file)
{
    static int counter = 0;

    if (kfetch_Open)
        return -EBUSY;

    kfetch_Open++;
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int kfetch_release(struct inode *inode, struct file *file)
{
    kfetch_Open--;          /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return SUCCESS;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t kfetch_read(struct file *filp,   /* see include/linux/fs.h   */
                           char __user *buffer,        /* buffer to fill with data */
                           size_t length,       /* length of the buffer     */
                           loff_t * offset)
{
    if (copy_to_user(buffer, kfetch_buf , len_read)) {
        pr_alert("Failed to copy data to user\n");
        return 0;
    }
 
    return len_read;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/hello
 */
static ssize_t kfetch_write(struct file *filp,
                            const char __user *buffer,
                            size_t length,
                            loff_t * off)
{
    
    int mask_info;
    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user\n");
        return 0;
    }
    /* setting the informaion mask*/
    info_mask.mask = mask_info; 

    struct  sysinfo sys_info;
    unsigned long total_ram, free_ram;
    si_meminfo(&sys_info);

    total_ram=sys_info.totalram*PAGE_SIZE;
    total_ram=total_ram/1000000;
    free_ram=sys_info.freeram*PAGE_SIZE;
    free_ram=free_ram/1000000;

    unsigned int cpu = 0;
    struct cpuinfo_x86 *c;
    c=&cpu_data(cpu);

    char num_cpus[16];
    char mem_info[32];
    char uptime[16];
    char num_procs[16];
    int uptime_bf =jiffies_to_msecs(jiffies)/1000;
    sprintf(num_cpus, "%d / %d", num_online_cpus(), num_active_cpus());
    sprintf(mem_info, "%lu MB / %lu MB", free_ram, total_ram);
    sprintf(num_procs, "%d ", sys_info.procs);
    sprintf(uptime, "%ld mins", (uptime_bf / 60));

    info.release = utsname()->release;
    info.cpu_model = c->x86_model_id;
    info.num_cpus = num_cpus;
    info.mem = mem_info;
    info.uptime = uptime;
    info.num_procs = num_procs;
    
    int len = 0;
    char *draw[8];
    draw[0] =  "                     ";
    draw[1] =  "        .-.          ";
    draw[2] =  "       (.. |         ";
    draw[3] =  "       <>  |         ";
    draw[4] =  "      / --- \\        ";
    draw[5] =  "     ( |   | |       ";
    draw[6] =  "   |\\_)___/\\)/\\      ";
    draw[7] =  "  <__)------(__/     ";

    char info_BUF0[64]; 
    char info_BUF1[64]; 
    char info_BUF2[64]; 
    char info_BUF3[64]; 
    char info_BUF4[64]; 
    char info_BUF5[64]; 
    sprintf(info_BUF0,"Kernel : %s",info.release);
    sprintf(info_BUF1,"CPU : %s",info.cpu_model);
    sprintf(info_BUF2,"CPUs : %s",info.num_cpus);
    sprintf(info_BUF3,"Mem : %s",info.mem);
    sprintf(info_BUF4,"Procs : %s",info.num_procs);
    sprintf(info_BUF5,"uptime : %s",info.uptime);
    
    char *info_fun[6];
    info_fun[0]=info_BUF0;
    info_fun[1]=info_BUF1;
    info_fun[2]=info_BUF2;
    info_fun[3]=info_BUF3;
    info_fun[4]=info_BUF4;
    info_fun[5]=info_BUF5;

    len+= sprintf(kfetch_buf+len, "%s %s\n",draw[0],init_uts_ns.name.nodename);
    len+= sprintf(kfetch_buf+len, "%s %s\n",draw[1],"----------------");

    /*1 work, 0 not work*/
    int countarray [6];
    if (info_mask.mask & KFETCH_RELEASE) {
        countarray[0] = 1;
    }
    else{
        countarray[0] = 0;
    }
    if (info_mask.mask & KFETCH_CPU_MODEL) {
        countarray[1] = 1;
    }
    else{
        countarray[1] = 0;
    }
    if (info_mask.mask& KFETCH_NUM_CPUS) {
        countarray[2] = 1;
    }
    else{
        countarray[2] = 0;
    }
    if (info_mask.mask & KFETCH_MEM) {
        countarray[3] = 1;
    }
    else{
        countarray[3] = 0;
    }

    if (info_mask.mask & KFETCH_NUM_PROCS) {
        countarray[4] = 1;
    }
    else{
        countarray[4] = 0;
    }
    if (info_mask.mask & KFETCH_UPTIME) {
        countarray[5] = 1;   
    }
    else{
        countarray[5] = 0;
    }

    int count=0;
    for (int i=2; i<=7; i++)
    {   
        
        for (int j=0; j<=5; j++)
        {
            if (countarray[j+count]==1  && ((j+count) <=5))
            {
                len+= sprintf(kfetch_buf+len, "%s %s\n",draw[i],info_fun[j+count]);
                count = count + j +1;
                break;
            }
            else
            {
                if (j==5)
                {
                    len+= sprintf(kfetch_buf+len, "%s\n",draw[i]);
                    break;
                }
                else
                    continue;
            }

        }
    }
    len_read=len;

    return length;
  
}


module_init(kfetch_init);
module_exit(kfetch_cleanup);

MODULE_AUTHOR("Benson");
MODULE_DESCRIPTION("Print CPU information");
MODULE_LICENSE("GPL");
