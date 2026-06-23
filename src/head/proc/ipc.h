#ifndef IPC_H
#define IPC_H

#include "harlin_API.h"

#define IPC_MAX_QUEUES         16
#define IPC_MAX_MSG_PER_QUEUE  16
#define IPC_MSG_DATA_SIZE      64
#define IPC_MAX_SEMS           16

struct ipc_message {
    u32 type;
    u8 data[IPC_MSG_DATA_SIZE];
    int valid;
};

struct ipc_queue {
    int used;
    struct ipc_message msgs[IPC_MAX_MSG_PER_QUEUE];
    int head;
    int count;
    int blocked_senders[16];
    int num_blocked_senders;
    int blocked_receivers[16];
    int num_blocked_receivers;
};

struct ipc_semaphore {
    int used;
    int count;
    int blocked_procs[16];
    int num_blocked;
};

void ipc_init(void);

#define Harlin_IpcInit                ipc_init

int Harlin_MsgCreate(void);
int Harlin_MsgSend(int qid, u32 type, const void* data, u32 len);
int Harlin_MsgRecv(int qid, u32* type, void* buf, u32 len, u32 expected_type);
int Harlin_MsgDestroy(int qid);

int Harlin_SemCreate(int initial);
int Harlin_SemWait(int sid);
int Harlin_SemPost(int sid);
int Harlin_SemDestroy(int sid);

#endif
