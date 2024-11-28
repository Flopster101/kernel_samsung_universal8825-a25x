/*
  *
  **************************************************************************
  **                        STMicroelectronics				 **
  **************************************************************************
  **                        marco.cali@st.com				*
  **************************************************************************
  *                                                                        *
  *                     Utilities published in /proc/fts		*
  *                                                                        *
  **************************************************************************
  **************************************************************************
  *
  */

/*!
  * \file fts_proc.c
  * \brief contains the function and variables needed to publish a file node in
  * the file system which allow to communicate with the IC from userspace
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "stm_dev.h"

static struct stm_ts_data *proc_ts;

char tag[12] = "[fts_touch]\0";

#define FTS_TS_DRV_VER		0x05021300	/* driver version u32 format */

/* FIRST LEVEL ERROR CODE */
/** @defgroup first_level	First Level Error Code
  * @ingroup error_codes
  * Errors related to low level operation which are not under control of driver,
  * such as: communication protocol (I2C/SPI), timeout, file operations ...
  * @{
  */
#define OK				((int)0x00000000)	/* /< No ERROR */
#define ERROR_ALLOC		((int)0x80000001)	/* /< allocation of memory failed */
#define ERROR_BUS_R		((int)0x80000002)	/* /< i2c/spi read failed */
#define ERROR_BUS_W		((int)0x80000003)	/* /< i2c/spi write failed */
#define ERROR_BUS_WR	((int)0x80000004)	/* /< i2c/spi write/read failed */
#define ERROR_BUS_O		((int)0x80000005)	/* /< error during opening an i2c device */
#define ERROR_OP_NOT_ALLOW	((int)0x80000006)	/* /< operation not allowed */
#define ERROR_TIMEOUT		((int)0x80000007)	/* /< timeout expired! exceed the max number of retries or the max waiting time */

#define DRIVER_TEST_FILE_NODE	"driver_test"	/* /< name of file node published */
#define CHUNK_PROC				1024	/* /< Max chunk of data printed on the sequential file in each iteration */
#define DIAGNOSTIC_NUM_FRAME	10	/* /< number of frames reading iterations during the diagnostic test */

/** @}*/

/** @defgroup scriptless Scriptless Protocol
  * @ingroup proc_file_code
  * Scriptless Protocol allows ST Software (such as FingerTip Studio etc) to
  * communicate with the IC from an user space.
  * This mode gives access to common bus operations (write, read etc) and
  * support additional functionalities. \n
  * The protocol is based on exchange of binary messages included between a
  * start and an end byte
  * @{
  */

#define MESSAGE_START_BYTE		0x7B	/* /< start byte of each message transferred in Scriptless Mode */
#define MESSAGE_END_BYTE		0x7D	/* /< end byte of each message transferred in Scriptless Mode */
#define MESSAGE_MIN_HEADER_SIZE 8	/* /< minimun number of bytes of the structure of a messages exchanged with host (include start/end byte, counter, actions, msg_size) */

/* GUI utils byte ver */
#define CMD_WRITE_BYTE			0xF1	/* /< Byte output version of I2C/SPI write @see CMD_WRITE */
#define CMD_WRITEREAD_BYTE		0xF2	/* /< Byte output version of I2C/SPI writeRead @see CMD_WRITEREAD */
#define CMD_VERSION_BYTE		0xFA	/* /< Byte output version of driver version and setting @see CMD_VERSION */

/**
  * Possible actions that can be requested by an host
  */
typedef enum {
	ACTION_WRITE			= (u16) 0x0001,	/* /< Bus Write */
	ACTION_READ				= (u16) 0x0002,	/* /< Bus Read */
	ACTION_WRITE_READ		= (u16) 0x0003,	/* /< Bus Write followed by a Read */
	ACTION_GET_VERSION		= (u16) 0x0004,	/* /< Get Version of the protocol (equal to the first 2 bye of driver version) */
} Actions;

/**
  * Struct used to contain info of the message received by the host in
  * Scriptless mode
*/

typedef struct {
	u16 msg_size;	/* /< total size of the message in bytes */
	u16 counter;	/* /< counter ID to identify a message */
	Actions action;	/* /< type of operation requested by the host see Actions */
	u8 dummy;		/* /< (optional)in case of any kind of read operations, specify if the first byte is dummy */
} Message;

/** @}*/

static int limit; /* /< store the amount of data to print into the shell */
static int chunk;	/* /< store the chuk of data that should be printed in this iteration */
static int printed;	/* /< store the amount of data already printed in the shell */
static struct proc_dir_entry *fts_dir;	/* /< reference to the directory fts under /proc */
static u8 *driver_test_buff;	/* /< pointer to an array of bytes used to store the result of the function executed */
char buf_chunk[CHUNK_PROC] = { 0 };	/* /< buffer used to store the message info received */
static Message mess = { 0 };	/* /< store the information of the Scriptless message received */

/************************ SEQUENTIAL FILE UTILITIES **************************/
/**
  * This function is called at the beginning of the stream to a sequential file
  * or every time into the sequential were already written PAGE_SIZE bytes and
  * the stream need to restart
  * @param s pointer to the sequential file on which print the data
  * @param pos pointer to the offset where write the data
  * @return NULL if there is no data to print or the pointer to the beginning of
  * the data that need to be printed
  */
static void *fts_seq_start(struct seq_file *s, loff_t *pos)
{
	// pr_info("%s %s: Entering start(), pos = %ld limit = %d printed = %d\n", tag, __func__, *pos, limit, printed);

	if (driver_test_buff == NULL && *pos == 0) {
		pr_info("%s %s: No data to print!\n", tag, __func__);

		driver_test_buff = (u8 *)kzalloc(13 * sizeof(u8), GFP_KERNEL);

		snprintf(driver_test_buff, 14, "{ %08X }\n", ERROR_OP_NOT_ALLOW);


		limit = strlen(driver_test_buff);
		/* pr_info("%s %s: len = %d driver_test_buff = %s\n", tag, __func__, limit, driver_test_buff); */
	} else {
		if (*pos != 0)
			*pos += chunk - 1;

		if (*pos >= limit)
			/* pr_info("%s %s: Apparently, we're done.\n", tag, __func__); */
			return NULL;
	}

	chunk = CHUNK_PROC;
	if (limit - *pos < CHUNK_PROC)
		chunk = limit - *pos;

	/* pr_info("%s %s: In start(), updated pos = %ld limit = %d printed = %d chunk = %d\n", tag, __func__, *pos, limit, printed, chunk); */
	memset(buf_chunk, 0, CHUNK_PROC);
	memcpy(buf_chunk, &driver_test_buff[(int)*pos], chunk);

	return buf_chunk;
}

/**
  * This function actually print a chunk amount of data in the sequential file
  * @param s pointer to the sequential file where to print the data
  * @param v pointer to the data to print
  * @return 0
  */
static int fts_seq_show(struct seq_file *s, void *v)
{
	/* pr_info("%s %s: In show()\n", tag, __func__); */
	seq_write(s, (u8 *)v, chunk);
	printed += chunk;
	return 0;
}

/**
  * This function update the pointer and the counters to the next data to be
  * printed
  * @param s pointer to the sequential file where to print the data
  * @param v pointer to the data to print
  * @param pos pointer to the offset where write the next data
  * @return NULL if there is no data to print or the pointer to the beginning of
  * the next data that need to be printed
  */
static void *fts_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	/* int* val_ptr; */
	/* pr_info("%s %s: In next(), v = %X, pos = %ld.\n", tag, __func__, v, *pos); */
	(*pos) += chunk;	/* increase my position counter */
	chunk = CHUNK_PROC;

	/* pr_info("%s %s: In next(), updated pos = %ld limit = %d printed = %d\n", tag, __func__, *pos, limit,printed); */
	if (*pos >= limit)	/* are we done? */
		return NULL;
	else if (limit - *pos < CHUNK_PROC)
		chunk = limit - *pos;


	memset(buf_chunk, 0, CHUNK_PROC);
	memcpy(buf_chunk, &driver_test_buff[(int)*pos], chunk);
	return buf_chunk;
}


/**
  * This function is called when there are no more data to print  the stream
  *need to be terminated or when PAGE_SIZE data were already written into the
  *sequential file
  * @param s pointer to the sequential file where to print the data
  * @param v pointer returned by fts_seq_next
  */
static void fts_seq_stop(struct seq_file *s, void *v)
{
	/* pr_info("%s %s: Entering stop().\n", tag, __func__); */

	if (v) {
		/* pr_info("%s %s: v is %X.\n", tag, __func__, v); */
	} else {
		/* pr_info("%s %s: v is null.\n", tag, __func__); */
		limit = 0;
		chunk = 0;
		printed = 0;
		if (driver_test_buff != NULL) {
			/* pr_info("%s %s: Freeing and clearing driver_test_buff.\n", tag, __func__); */
			kfree(driver_test_buff);
			driver_test_buff = NULL;
		} else {
			/* pr_info("%s %s: driver_test_buff is already null.\n", tag, __func__); */
		}
	}
}

/**
  * Struct where define and specify the functions which implements the flow for
  *writing on a sequential file
  */
static const struct seq_operations fts_seq_ops = {
	.start	= fts_seq_start,
	.next	= fts_seq_next,
	.stop	= fts_seq_stop,
	.show	= fts_seq_show
};

/**
  * This function open a sequential file
  * @param inode Inode in the file system that was called and triggered this
  * function
  * @param file file associated to the file node
  * @return error code, 0 if success
  */
static int fts_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fts_seq_ops);
};


/*****************************************************************************/

/**************************** DRIVER TEST ************************************/

/** @addtogroup proc_file_code
  * @{
  */

/**
  * Receive the OP code and the inputs from shell when the file node is called,
  * parse it and then execute the corresponding function
  * echo cmd+parameters > /proc/fts/driver_test to execute the select command
  * cat /proc/fts/driver_test			to obtain the result into the
  * shell \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * the answer content and format strictly depend on the cmd executed. In
  * general can be: an HEX string or a byte array (e.g in case of 0xF- commands)
  * \n
  * } = end byte \n
  */
static ssize_t fts_driver_test_write(struct file *file, const char __user *buf,
				     size_t count, loff_t *pos)
{
	int numberParam = 0;
	char *p = NULL;
	int res = -1, j, index = 0;
	int size = 6;
	int temp, byte_call = 0;
	u16 byteToRead = 0;
	u8 *readData = NULL;
	u8 *pbuf = NULL;
	u8 *cmd;
	u32 *funcToTest;

	pbuf = kzalloc(count, GFP_KERNEL);
	cmd = kzalloc(count, GFP_KERNEL);
	funcToTest = kzalloc((((count + 1) / 3) * 4), GFP_KERNEL);;

	mess.dummy = 0;
	mess.action = 0;
	mess.msg_size = 0;

	if (access_ok(buf, count) < OK || copy_from_user(pbuf, buf, count) != 0) {
		res = ERROR_ALLOC;
		goto END;
	}

	p = pbuf;
	if (count > MESSAGE_MIN_HEADER_SIZE - 1 && p[0] == MESSAGE_START_BYTE) {
		byte_call = 1;
		mess.msg_size = (p[1] << 8) | p[2];
		mess.counter = (p[3] << 8) | p[4];
		mess.action = (p[5] << 8) | p[6];

		size = MESSAGE_MIN_HEADER_SIZE + 2;	/* +2 error code */
		if (count < mess.msg_size || p[count - 2] != MESSAGE_END_BYTE) {
			pr_info("%s number of byte received or end byte wrong! msg_size = %d != %d, last_byte = %02X != %02X ... ERROR %08X\n",
				 tag, mess.msg_size, (int)count, p[count - 1], MESSAGE_END_BYTE, ERROR_OP_NOT_ALLOW);
			res = ERROR_OP_NOT_ALLOW;
			goto END;
		} else {
			numberParam = mess.msg_size - MESSAGE_MIN_HEADER_SIZE + 1;	/* +1 because put the internal op code */
			size = MESSAGE_MIN_HEADER_SIZE + 2;	/* +2 send also the first 2 lsb of the error code */
			switch (mess.action) {
			case ACTION_WRITE:
				cmd[0] = funcToTest[0] = CMD_WRITE_BYTE;
				break;
			case ACTION_WRITE_READ:
				cmd[0] = funcToTest[0] = CMD_WRITEREAD_BYTE;
				break;
			case ACTION_GET_VERSION:
				cmd[0] = funcToTest[0] = CMD_VERSION_BYTE;
				break;
			default:
				pr_info("%s Invalid Action = %d ... ERROR %08X\n", tag, mess.action, ERROR_OP_NOT_ALLOW);
				res = ERROR_OP_NOT_ALLOW;
				goto END;
			}

			if (numberParam - 1 != 0)
				memcpy(&cmd[1], &p[7], numberParam - 1);
			/* -1 because i need to exclude the cmd[0] */
		}
	} else {
		if (((count + 1) / 3) >= 1) {
			if (sscanf(p, "%02X ", &funcToTest[0]) == 1) {
				p += 3;
				cmd[0] = (u8)funcToTest[0];
				numberParam = 1;
			}
		} else {
			res = ERROR_OP_NOT_ALLOW;
			goto END;
		}

		for (; numberParam < (count + 1) / 3; numberParam++) {
			if (sscanf(p, "%02X ", &funcToTest[numberParam]) == 1) {
				p += 3;
				cmd[numberParam] = (u8)funcToTest[numberParam];
			}
		}
	}

	/* elaborate input */
	if (numberParam >= 1) {
		switch (funcToTest[0]) {
		case CMD_VERSION_BYTE:
			byteToRead = 2;
			mess.dummy = 0;
			readData = (u8 *)kzalloc(byteToRead * sizeof(u8), GFP_KERNEL);
			if (!readData) {
				pr_info("%s %s: error to alloc readData\n", tag, __func__);
				return -ENOMEM;
			}
			size += byteToRead;
			if (readData != NULL) {
				readData[0] = (u8)(FTS_TS_DRV_VER >> 24);
				readData[1] = (u8)(FTS_TS_DRV_VER >> 16);
				res = OK;
				pr_info("%s %s: Version = %02X%02X\n", tag, __func__, readData[0], readData[1]);
			} else {
				res = ERROR_ALLOC;
				pr_info("%s %s: Impossible allocate memory... ERROR %08X\n", tag, __func__, res);
			}
			break;
		case CMD_WRITEREAD_BYTE:
			if (numberParam >= 5) {	/* need to pass: cmd[0]  cmd[1] … cmd[cmdLength-1] byteToRead1 byteToRead0 dummyByte */
				temp = numberParam - 4;
				if (cmd[numberParam - 1] == 0)
					mess.dummy = 0;
				else
					mess.dummy = 1;

				byteToRead = (u16)(((cmd[numberParam - 3 + 0] & 0x00FF) << 8) + (cmd[numberParam - 3 + 1] & 0x00FF));

				readData = (u8 *)kzalloc((byteToRead + mess.dummy + 1) * sizeof(u8), GFP_KERNEL);
				if (!readData) {
					pr_info("%s %s: error to alloc readData\n", tag, __func__);
					return -ENOMEM;
				}
					res = proc_ts->stm_ts_read(proc_ts, &cmd[1], temp, &readData[1], byteToRead + mess.dummy);
					size += (byteToRead * sizeof(u8));
			} else {
				pr_info("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;
		case CMD_WRITE_BYTE:
			if (numberParam >= 2) {	/* need to pass: cmd[0]  cmd[1] … cmd[cmdLength-1] */
				temp = numberParam - 1;
				res = proc_ts->stm_ts_write(proc_ts, &cmd[1], temp, NULL, 0);
			} else {
				pr_info("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;
		default:
			pr_info("%s COMMAND ID NOT VALID!!!\n", tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}
	} else {
		pr_info("%s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n", tag);
		res = ERROR_OP_NOT_ALLOW;
	}

END:	/* here start the reporting phase, assembling the data to send in the
	 * file node */
	if (driver_test_buff != NULL) {
		pr_info("%s Consecutive echo on the file node, free the buffer with the previous result\n", tag);
		kfree(driver_test_buff);
	}

	if (byte_call == 0) {
		size *= 2;
		size += 2;	/* add \n and \0 (terminator char) */
	} else {
		size *= 2; /* need to code each byte as HEX string */
		size -= 1;	/* start byte is just one, the extra
					* byte of end byte taken by \n */
	}
	driver_test_buff = (u8 *)kzalloc(size, GFP_KERNEL);

	if (driver_test_buff == NULL) {
		pr_info("%s Unable to allocate driver_test_buff! ERROR %08X\n", tag, ERROR_ALLOC);
		goto ERROR;
	}
	/* start byte */
	driver_test_buff[index++] = MESSAGE_START_BYTE;

	snprintf(&driver_test_buff[index], 5, "%02X%02X", (size & 0xFF00) >> 8,	size & 0xFF);
	index += 4;
	index += snprintf(&driver_test_buff[index], 5, "%04X", (u16)mess.counter);
	index += snprintf(&driver_test_buff[index], 5, "%04X", (u16)mess.action);
	index += snprintf(&driver_test_buff[index], 5, "%02X%02X", (res & 0xFF00) >> 8, res & 0xFF);

	switch (funcToTest[0]) {
	case CMD_VERSION_BYTE:
	case CMD_WRITEREAD_BYTE:
		j = mess.dummy;
		for (; j < byteToRead + mess.dummy; j++)
			index += snprintf(&driver_test_buff[index], 3, "%02X", (u8)readData[j]);
		break;
	default:
		break;
	}

	driver_test_buff[index++] = MESSAGE_END_BYTE;
	driver_test_buff[index] = '\n';

	limit = size;
	printed = 0;
ERROR:
	numberParam = 0;/* need to reset the number of parameters in order to
			 * wait the next command, comment if you want to repeat
			 * the last comand sent just doing a cat */

	/* pr_info("%s numberParameters = %d\n",tag, numberParam); */
	if (readData != NULL)
		kfree(readData);

	kfree(pbuf);
	kfree(cmd);
	kfree(funcToTest);

	return count;
}

/** @}*/

/**
  * file_operations struct which define the functions for the canonical
  *operation on a device file node (open. read, write etc.)
  */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops fts_driver_test_ops = {
	.proc_open	= fts_open,
	.proc_read	= seq_read,
	.proc_write	= fts_driver_test_write,
	//.unlocked_ioctl = driver_ioctl,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release
};
#else
static const struct file_operations fts_driver_test_ops = {
	.open		= fts_open,
	.read		= seq_read,
	.write		= fts_driver_test_write,
	//.unlocked_ioctl = driver_ioctl,
	.llseek		= seq_lseek,
	.release	= seq_release
};
#endif

/*****************************************************************************/

/**
  * This function is called in the probe to initialize and create the directory
  * proc/fts and the driver test file node DRIVER_TEST_FILE_NODE into the /proc
  * file system
  * @return OK if success or an error code which specify the type of error
  */
int stm_ts_tool_proc_init(struct stm_ts_data *ts)
{
	struct proc_dir_entry *entry;

	int retval = 0;

	proc_ts = (struct stm_ts_data *)ts;

	fts_dir = proc_mkdir_data("fts", 0777, NULL, NULL);
	if (fts_dir == NULL) {	/* directory creation failed */
		retval = -ENOMEM;
		goto out;
	}

	entry = proc_create(DRIVER_TEST_FILE_NODE, 0777, fts_dir, &fts_driver_test_ops);

	if (entry)
		pr_info("%s %s: proc entry CREATED!\n", tag, __func__);
	else {
		pr_info("%s %s: error creating proc entry!\n", tag, __func__);
		retval = -ENOMEM;
		goto badfile;
	}
	return OK;
badfile:
	remove_proc_entry("fts", NULL);
out:
	return retval;
}

/**
  * Delete and Clean from the file system, all the references to the driver test
  * file node
  * @return OK
  */
int stm_ts_tool_proc_remove(void)
{
	remove_proc_entry(DRIVER_TEST_FILE_NODE, fts_dir);
	remove_proc_entry("fts", NULL);
	return OK;
}
