/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_COMM_H__
#define __SHUB_COMM_H__

#include <linux/kernel.h>
#include "shub_cmd.h"

struct shub_msg {
	u8 cmd;
	u8 type;
	u8 subcmd;
	u16 total_length;
	u16 length;
	u64 timestamp;
	char *buffer;
	bool is_empty_pending_list;
	struct completion *done;
	struct list_head list;
} __attribute__((__packed__));

#define SHUB_CMD_SIZE			64
#define SHUB_MSG_HEADER_SIZE	offsetof(struct shub_msg, buffer)
#define SHUB_MSG_BUFFER_SIZE	(SHUB_CMD_SIZE - SHUB_MSG_HEADER_SIZE)

int shub_send_command(u8 cmd, u8 type, u8 subcmd, char *send_buf, int send_buf_len);
int shub_send_command_wait(u8 cmd, u8 type, u8 subcmd, int timeout, char *send_buf, int send_buf_len,
			   char **receive_buf, int *receive_buf_len, bool reset);
void handle_packet(char *packet, int packet_size);

int get_cnt_comm_fail(void);
int get_cnt_timeout(void);

int init_comm_to_hub(void);
void exit_comm_to_hub(void);

void stop_comm_to_hub(void);

#endif /* __SHUB_COMM_H__ */
