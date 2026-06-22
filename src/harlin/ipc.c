#include "ipc.h"
#include "scheduler.h"

static struct ipc_queue queues[IPC_MAX_QUEUES];
static struct ipc_semaphore semaphores[IPC_MAX_SEMS];

void ipc_init(void)
{
    int i;
    for (i = 0; i < IPC_MAX_QUEUES; i++)
        queues[i].used = 0;
    for (i = 0; i < IPC_MAX_SEMS; i++)
        semaphores[i].used = 0;
}

static int find_free_queue(void)
{
    int i;
    for (i = 0; i < IPC_MAX_QUEUES; i++) {
        if (!queues[i].used)
            return i;
    }
    return -1;
}

static int find_free_sem(void)
{
    int i;
    for (i = 0; i < IPC_MAX_SEMS; i++) {
        if (!semaphores[i].used)
            return i;
    }
    return -1;
}

int Harlin_MsgCreate(void)
{
    int qid = find_free_queue();
    if (qid < 0)
        return -1;
    queues[qid].used = 1;
    queues[qid].head = 0;
    queues[qid].count = 0;
    queues[qid].num_blocked_senders = 0;
    queues[qid].num_blocked_receivers = 0;
    return qid;
}

int Harlin_MsgSend(int qid, u32 type, const void* data, u32 len)
{
    int tail;
    struct ipc_queue* q;
    int i;

    if (qid < 0 || qid >= IPC_MAX_QUEUES || !queues[qid].used)
        return -1;
    q = &queues[qid];

    if (len > IPC_MSG_DATA_SIZE)
        return -1;

    while (q->count >= IPC_MAX_MSG_PER_QUEUE) {
        if (q->num_blocked_receivers > 0) {
            int pid = q->blocked_receivers[0];
            for (i = 1; i < q->num_blocked_receivers; i++)
                q->blocked_receivers[i - 1] = q->blocked_receivers[i];
            q->num_blocked_receivers--;
            process_wake(pid);
            continue;
        }

        if (q->num_blocked_senders >= 16)
            return -1;

        {
            struct process* cur = process_current();
            q->blocked_senders[q->num_blocked_senders++] = cur->pid;
        }
        process_block_current();
    }

    tail = (q->head + q->count) % IPC_MAX_MSG_PER_QUEUE;
    q->msgs[tail].type = type;
    q->msgs[tail].valid = 1;
    {
        const u8* src = data;
        for (i = 0; i < (int)len; i++)
            q->msgs[tail].data[i] = src[i];
    }
    q->count++;

    if (q->num_blocked_receivers > 0) {
        int pid = q->blocked_receivers[0];
        for (i = 1; i < q->num_blocked_receivers; i++)
            q->blocked_receivers[i - 1] = q->blocked_receivers[i];
        q->num_blocked_receivers--;
        process_wake(pid);
    }

    return 0;
}

int Harlin_MsgRecv(int qid, u32* type, void* buf, u32 len, u32 expected_type)
{
    int i;
    struct ipc_queue* q;
    struct process* cur;

    if (qid < 0 || qid >= IPC_MAX_QUEUES || !queues[qid].used)
        return -1;
    q = &queues[qid];

    if (!type || !buf)
        return -1;

    cur = process_current();

    for (;;) {
        if (q->count > 0) {
            int found = 0;
            int msg_idx = -1;
            int k;

            for (k = 0; k < q->count; k++) {
                int idx = (q->head + k) % IPC_MAX_MSG_PER_QUEUE;
                if (q->msgs[idx].valid && (expected_type == 0 || q->msgs[idx].type == expected_type)) {
                    found = 1;
                    msg_idx = idx;
                    break;
                }
            }

            if (found) {
                *type = q->msgs[msg_idx].type;
                {
                    u8* dst = buf;
                    u32 copy_len = (len > IPC_MSG_DATA_SIZE) ? IPC_MSG_DATA_SIZE : len;
                    for (i = 0; i < (int)copy_len; i++)
                        dst[i] = q->msgs[msg_idx].data[i];
                }
                q->msgs[msg_idx].valid = 0;
                q->head = (msg_idx + 1) % IPC_MAX_MSG_PER_QUEUE;
                q->count--;

                if (q->num_blocked_senders > 0) {
                    int pid = q->blocked_senders[0];
                    for (i = 1; i < q->num_blocked_senders; i++)
                        q->blocked_senders[i - 1] = q->blocked_senders[i];
                    q->num_blocked_senders--;
                    process_wake(pid);
                }

                return 0;
            }
        }

        if (q->num_blocked_receivers >= 16)
            return -1;

        q->blocked_receivers[q->num_blocked_receivers++] = cur->pid;
        process_block_current();
    }
}

int Harlin_MsgDestroy(int qid)
{
    int i;
    if (qid < 0 || qid >= IPC_MAX_QUEUES || !queues[qid].used)
        return -1;

    for (i = 0; i < queues[qid].num_blocked_senders; i++)
        process_wake(queues[qid].blocked_senders[i]);
    for (i = 0; i < queues[qid].num_blocked_receivers; i++)
        process_wake(queues[qid].blocked_receivers[i]);

    queues[qid].used = 0;
    return 0;
}

int Harlin_SemCreate(int initial)
{
    int sid = find_free_sem();
    if (sid < 0)
        return -1;
    semaphores[sid].used = 1;
    semaphores[sid].count = (initial < 0) ? 0 : initial;
    semaphores[sid].num_blocked = 0;
    return sid;
}

int Harlin_SemWait(int sid)
{
    struct ipc_semaphore* s;
    struct process* cur;

    if (sid < 0 || sid >= IPC_MAX_SEMS || !semaphores[sid].used)
        return -1;
    s = &semaphores[sid];

    cur = process_current();

    while (s->count <= 0) {
        if (s->num_blocked >= 16)
            return -1;
        s->blocked_procs[s->num_blocked++] = cur->pid;
        process_block_current();
    }

    s->count--;
    return 0;
}

int Harlin_SemPost(int sid)
{
    int i;
    if (sid < 0 || sid >= IPC_MAX_SEMS || !semaphores[sid].used)
        return -1;

    semaphores[sid].count++;

    if (semaphores[sid].num_blocked > 0) {
        int pid = semaphores[sid].blocked_procs[0];
        for (i = 1; i < semaphores[sid].num_blocked; i++)
            semaphores[sid].blocked_procs[i - 1] = semaphores[sid].blocked_procs[i];
        semaphores[sid].num_blocked--;
        process_wake(pid);
    }

    return 0;
}

int Harlin_SemDestroy(int sid)
{
    int i;
    if (sid < 0 || sid >= IPC_MAX_SEMS || !semaphores[sid].used)
        return -1;

    for (i = 0; i < semaphores[sid].num_blocked; i++)
        process_wake(semaphores[sid].blocked_procs[i]);

    semaphores[sid].used = 0;
    return 0;
}
