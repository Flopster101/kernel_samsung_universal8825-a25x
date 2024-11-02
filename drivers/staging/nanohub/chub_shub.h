#ifndef _CHUB_SHUB_H_
#define _CHUB_SHUB_H_

void set_contexthub_info(struct contexthub_ipc_info *chub);
void contexthub_dump_notifier_call(int val, struct contexthub_dump *dump);
void contexthub_ipc_notifier_call(int val, struct contexthub_ipc_data *data);

#endif