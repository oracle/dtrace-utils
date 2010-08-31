#ifndef TYPES_DEV_H
#define TYPES_DEV_H
/*
 * For compatibility reasons the following typedefs (prefixed o_)
 * can't grow regardless of the EFT definition. Although,
 * applications should not explicitly use these typedefs
 * they may be included via a system header definition.
 * WARNING: These typedefs may be removed in a future
 * release.
 *              ex. the definitions in s5inode.h (now obsoleted)
 *                      remained small to preserve compatibility
 *                      in the S5 file system type.
 */
typedef ushort_t o_mode_t;              /* old file attribute type */
typedef short   o_dev_t;                /* old device type      */
typedef ushort_t o_uid_t;               /* old UID type         */
typedef o_uid_t o_gid_t;                /* old GID type         */
typedef short   o_nlink_t;              /* old file link type   */
typedef short   o_pid_t;                /* old process id type  */
typedef ushort_t o_ino_t;               /* old inode type       */

#endif
