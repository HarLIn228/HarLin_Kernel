#include "perm.h"
#include "printk.h"
#include "bug.h"

int Harlin_PermCheck(u16 mode, u16 file_owner, u16 file_group,
                     u16 uid, u16 gid, int want)
{
    u16 mask;
    if (uid == file_owner) {
        mask = (u16)((mode >> 6) & 0x7);
    } else if (gid == file_group) {
        mask = (u16)((mode >> 3) & 0x7);
    } else {
        mask = (u16)(mode & 0x7);
    }
    return (mask & (u16)want) == (u16)want;
}

int Harlin_PermTest(void)
{
    u16 mode = PERM_DEFAULT_FILE;
    ASSERT(Harlin_PermCheck(mode, 100, 10, 100, 10, PERM_WANT_READ));
    ASSERT(Harlin_PermCheck(mode, 100, 10, 100, 10, PERM_WANT_WRITE));
    ASSERT(!Harlin_PermCheck(mode, 100, 10, 100, 10, PERM_WANT_RUN));

    ASSERT(Harlin_PermCheck(mode, 100, 10, 200, 10, PERM_WANT_READ));
    ASSERT(!Harlin_PermCheck(mode, 100, 10, 200, 10, PERM_WANT_WRITE));

    ASSERT(Harlin_PermCheck(mode, 100, 10, 200, 20, PERM_WANT_READ));
    ASSERT(!Harlin_PermCheck(mode, 100, 10, 200, 20, PERM_WANT_WRITE));

    mode = 0;
    ASSERT(!Harlin_PermCheck(mode, 100, 10, 100, 10, PERM_WANT_READ));
    ASSERT(!Harlin_PermCheck(mode, 100, 10, 200, 10, PERM_WANT_READ));
    ASSERT(!Harlin_PermCheck(mode, 100, 10, 200, 20, PERM_WANT_READ));

    pr_info("perm: test OK");
    return 0;
}
