#ifndef PERM_H
#define PERM_H

#include "harlin_API.h"

#define PERM_OWN_R   (1u << 8)
#define PERM_OWN_W   (1u << 7)
#define PERM_OWN_X   (1u << 6)
#define PERM_GRP_R   (1u << 5)
#define PERM_GRP_W   (1u << 4)
#define PERM_GRP_X   (1u << 3)
#define PERM_OTH_R   (1u << 2)
#define PERM_OTH_W   (1u << 1)
#define PERM_OTH_X   (1u << 0)

#define PERM_DEFAULT_FILE (PERM_OWN_R | PERM_OWN_W | PERM_GRP_R | PERM_OTH_R)
#define PERM_DEFAULT_DIR  (PERM_OWN_R | PERM_OWN_W | PERM_OWN_X | \
                            PERM_GRP_R | PERM_GRP_X | \
                            PERM_OTH_R | PERM_OTH_X)

#define PERM_WANT_READ  0x4
#define PERM_WANT_WRITE 0x2
#define PERM_WANT_RUN   0x1

int Harlin_PermCheck(u16 mode, u16 file_owner, u16 file_group,
                     u16 uid, u16 gid, int want);
int Harlin_PermTest(void);

#endif
