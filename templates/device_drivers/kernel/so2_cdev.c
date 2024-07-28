/*
 * Character device drivers lab
 *
 * All tasks
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "../include/so2_cdev.h"

MODULE_DESCRIPTION("SO2 character device");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define LOG_LEVEL	KERN_INFO

#define MY_MAJOR		42
#define MY_MINOR		0
#define NUM_MINORS		1
#define MODULE_NAME		"so2_cdev"
#define MESSAGE			"hello\n"
#define IOCTL_MESSAGE		"Hello ioctl"

#ifndef BUFSIZ
#define BUFSIZ		4096
#endif

#define EXTRA


struct so2_device_data {
	/* add cdev member */
	struct cdev cdev;
	/* add buffer with BUFSIZ elements */
	char buffer[BUFSIZ];
	size_t size;
	/* extra members for home */
	wait_queue_head_t wq;
	int flag;
	/* add atomic_t access variable to keep track if file is opened */
	atomic_t access;
};

struct so2_device_data devs[NUM_MINORS];

static int so2_cdev_open(struct inode *inode, struct file *file)
{
	struct so2_device_data *data;

	/* print message when the device file is open. */
	printk(LOG_LEVEL "[%d] open called!\n",current->pid);

	/* inode->i_cdev contains our cdev struct, use container_of to obtain a pointer to so2_device_data */
	data = container_of(inode->i_cdev, struct so2_device_data, cdev);

	file->private_data = data;

#ifndef EXTRA
	/* return immediately if access is != 0, use atomic_cmpxchg */
	// atomic_cmpxchg用于比较一个原子变量的当前值与预期值，
	// 如果它们相等，则将该变量的值替换为新值，并返回旧值。
	// 如果它们不相等，就直接返回当前值
	if (atomic_cmpxchg(&data->access, 0, 1) != 0)
		return -EBUSY;
#endif

	// set_current_state(TASK_INTERRUPTIBLE);
	// schedule_timeout(10 * HZ);

	return 0;
}

static int
so2_cdev_release(struct inode *inode, struct file *file)
{
	/* print message when the device file is closed. */
	printk(LOG_LEVEL "[%d] close called!\n",current->pid);

#ifndef EXTRA
	struct so2_device_data *data =
		(struct so2_device_data *) file->private_data;

	/* reset access variable to 0, use atomic_set */
	atomic_set(&data->access, 0);
#endif
	return 0;
}

static ssize_t
so2_cdev_read(struct file *file,
		char __user *user_buffer,
		size_t size, loff_t *offset)
{
	struct so2_device_data *data =
		(struct so2_device_data *) file->private_data;
	size_t to_read;

#ifdef EXTRA
	/* extra tasks for home */
	if (!data->size) {
		/* O_NONBLOCK */
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(data->wq, data->size != 0))
			return -ERESTARTSYS;
	}
#endif

	/* Copy data->buffer to user_buffer, use copy_to_user */
	to_read = (size > data->size - *offset) ? (data->size - *offset) : size;
	if (copy_to_user(user_buffer, data->buffer + *offset, to_read) != 0)
		return -EFAULT;
	pr_info("Offset: %lld \n", *offset);
	// cat 命令一直读取到文件的末尾，文件通过读取返回值为 0 来表示读到末尾了。
	// 因此，为了正确实现，你需要更新并使用读函数中接收的偏移参数，并在用户达到缓冲区末尾时返回 0。	
	*offset += to_read;

	return to_read;
}

static ssize_t
so2_cdev_write(struct file *file,
		const char __user *user_buffer,
		size_t size, loff_t *offset)
{
	struct so2_device_data *data =
		(struct so2_device_data *) file->private_data;


	/* copy user_buffer to data->buffer, use copy_from_user */
	size = (*offset + size > BUFSIZ) ? (BUFSIZ - *offset) : size;
	if (copy_from_user(data->buffer + *offset, user_buffer, size) != 0)
		return -EFAULT;
	*offset += size;
	data->size = *offset;
	/* extra tasks for home */
#ifdef EXTRA
	wake_up_interruptible(&data->wq);
#endif

	return size;
}

static long
so2_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct so2_device_data *data =
		(struct so2_device_data *) file->private_data;
	int ret = 0;
	int remains;

	switch (cmd) {
	/* if cmd = MY_IOCTL_PRINT, display IOCTL_MESSAGE */
	case MY_IOCTL_PRINT:
		printk(LOG_LEVEL "%s\n", IOCTL_MESSAGE);
		break;
	/* extra tasks, for home */
	case MY_IOCTL_DOWN:
		data->flag = 0;
		ret = wait_event_interruptible(data->wq, data->flag != 0);
		break;
	case MY_IOCTL_UP:
		data->flag = 1;
		wake_up_interruptible(&data->wq);
		break;
	case MY_IOCTL_SET_BUFFER:
		remains = copy_from_user(data->buffer, (char __user *)arg,
				BUFFER_SIZE);
		if (remains)
			ret = -EFAULT;
		data->size = BUFFER_SIZE - remains;
		break;
	case MY_IOCTL_GET_BUFFER:
		if (copy_to_user((char __user *)arg, data->buffer, data->size))
			ret = -EFAULT;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations so2_fops = {
	.owner = THIS_MODULE,
/* add open and release functions */
	.open = so2_cdev_open,
	.release = so2_cdev_release,
/* add read function */
	.read = so2_cdev_read,
/* add write function */
	.write = so2_cdev_write,
/* add ioctl function */
	.unlocked_ioctl = so2_cdev_ioctl,
};

static int so2_cdev_init(void)
{
	int err;
	int i;

	/* register char device region for MY_MAJOR and NUM_MINORS starting at MY_MINOR */
	err = register_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR),
			NUM_MINORS, MODULE_NAME);
	if (err != 0) {
		pr_info("register_chrdev_region");
		return err;
	}

	for (i = 0; i < NUM_MINORS; i++) {
#ifdef EXTRA
		/* extra tasks, for home */
		devs[i].size = 0;
		memset(devs[i].buffer, 0, sizeof(devs[i].buffer));
#else
		/* initialize buffer with MESSAGE string */
		memcpy(devs[i].buffer, MESSAGE, sizeof(MESSAGE));
		devs[i].size = sizeof(MESSAGE);
		/* set access variable to 0, use atomic_set */
		atomic_set(&devs[i].access, 0);
#endif
		/* extra tasks for home */
		init_waitqueue_head(&devs[i].wq);
		devs[i].flag = 0;
		/* init and add cdev to kernel core */
		cdev_init(&devs[i].cdev, &so2_fops);
		cdev_add(&devs[i].cdev, MKDEV(MY_MAJOR, i), 1);
	}

	return 0;
}

static void so2_cdev_exit(void)
{
	int i;

	for (i = 0; i < NUM_MINORS; i++) {
		/* delete cdev from kernel core */
		cdev_del(&devs[i].cdev);
	}

	/* unregister char device region, for MY_MAJOR and NUM_MINORS starting at MY_MINOR */
	unregister_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), NUM_MINORS);
}

module_init(so2_cdev_init);
module_exit(so2_cdev_exit);
