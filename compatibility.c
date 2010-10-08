/**
 * @file
 * @author Vladimir Kolesnikov <vladimir@extrememember.com>
 * @version 0.4
 * @brief Compatibilty related stuff — implementation
 */

#include "compatibility.h"
#include <stddef.h>

/**
 * @details Uses @c setresuid() if available or @c seteuid() if not, that is why when @c method is not 0 only EUID is guaranteed to be set
 * (provided that the operation was successful)
 */
int my_setuids(uid_t ruid, uid_t euid, enum change_xid_mode_t mode)
{
	if (cxm_setuid == mode || cxm_setxid == mode) {
		return setuid(euid);
	}

#if HAVE_SETRESUID == 1
	return setresuid(ruid, euid, 0);
#elif HAVE_SETREUID == 1
	{
		int res;
		res = setreuid(ruid, -1);
		if (0 == res) {
			res = seteuid(euid);
		}

		return res;
	}
#else
	return seteuid(euid);
#endif

}

/**
 * @details Uses @c setresgid() if available or @c setegid() if not, that is why when @c method is not 0 only EGID is guaranteed to be set
 * (provided that the operation was successful)
 */
int my_setgids(gid_t rgid, gid_t egid, enum change_xid_mode_t mode)
{
	if (cxm_setxid == mode) {
		return setgid(egid);
	}

#if HAVE_SETRESGID == 1
	return setresgid(rgid, egid, 0);
#elif HAVE_SETREGID == 1
	{
		int res;
		res = setregid(rgid, -1);
		if (0 == res) {
			res = setegid(egid);
		}

		return res;
	}
#else
	return setegid(egid);
#endif
}

void my_getuids(uid_t* restrict ruid, uid_t* restrict euid)
{
	uid_t r, e;
#if HAVE_GETRESUID == 1
	uid_t dummy;
	getresuid(&r, &e, &dummy);
#else
	r = getuid();
	e = geteuid();
#endif

	if (NULL != ruid) *ruid = r;
	if (NULL != euid) *euid = e;
}

void my_getgids(gid_t* restrict rgid, gid_t* restrict egid)
{
	gid_t r, e;
#if HAVE_GETRESGID == 1
	gid_t dummy;
	getresuid(&r, &e, &dummy);
#else
	r = getgid();
	e = getegid();
#endif

	if (NULL != rgid) *rgid = r;
	if (NULL != egid) *egid = e;
}
