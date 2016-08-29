/* udev-lock.c: force linear serialization of module loading for RHEL.
 *
 *  Copyright (C) 2010 Jon Masters <jcm@jonmasters.org>.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "udev.h"

static int udev_lock_semid = -1;

/*
 * RHEL-specific. For various reasons, we desire to have calls to modprobe be
 * serialized in certain circumstances (specifically, as a means to avoid the
 * problems associated with network devices being enumerated out-of-order when
 * using parallel module loading techniques). There are longer term upstream
 * solutions, this particular file provides a temporary modprobe workaround.
 * It is enabled by default so as to give some users behavior they have come
 * to expect on RHEL systems, while hopefully not affecting others too much.
 *
 * The design uses a global signalling semaphore (mutex) to allow one single
 * instance of modprobe to execute in response to a RUN= rule event entry.
 *
 * NOTE: we need this weird IPC since file locks are unavailable if
 * the filesystem upon which we wish to store them is read-only.
 */

/* required to be defined for semctl */
union semun {
	int val; /* Value for SETVAL */
	struct semid_ds *buf; /* Buffer for IPC_STAT, IPC_SET */
	unsigned short *array; /* Array for GETALL, SETALL */
	struct seminfo *__buf; /* Buffer for IPC_INFO (Linux-specific) */
};

/*
 * udev_lock_init - initialize the global semaphore.
 */
int udev_lock_init(void)
{
	union semun arg; /* required for semctl */

	udev_lock_semid = semget(IPC_PRIVATE, 1, 0600);

	/* newly created */
	if (udev_lock_semid >= 0) {
		arg.val = 1; /* initial value - unlocked */

		/* V() */
		if (semctl(udev_lock_semid, 0, SETVAL, arg) == -1) {
			semctl(udev_lock_semid, 0, IPC_RMID);
			return -1;
		}

	/* already exists */
	} else {
		/*
		 * no need to have special case waiting logic since
		 * only one udevd instance will create one at a time
		 */
		return -1;
	}

	return 0;
}


unsigned int udev_lock_getncnt(void)
{
	int r;
	r = semctl(udev_lock_semid, 0, GETNCNT);
	return (r > 0 ? r : 0);
}

/*
 * udev_lock_teardown - tear down the global semaphore.
 * NOTE: This function is unused and implemented for completeness.
 */
int udev_lock_teardown(void)
{
	union semun arg;

	if (semctl(udev_lock_semid, 0, IPC_RMID, arg) == -1)
		return -1;

	return 0;
}

/*
 * udev_lock_down - grab the global semaphore
 */
int udev_lock_down(void) /* P() */
{
	struct sembuf sb;

	sb.sem_num = 0;
	sb.sem_op = -1; /* absolute, must be at least 1 - P() */
	sb.sem_flg = SEM_UNDO;

	if (semop(udev_lock_semid, &sb, 1) == -1)
		return -1;

	return 0;
}

/*
 * udev_lock_up - release the global semaphore
 */
int udev_lock_up(void) /* V() */
{
	struct sembuf sb;

	sb.sem_num = 0;
	sb.sem_op = 1; /* absolute, adds 1 - V() */
	sb.sem_flg = SEM_UNDO;

	if (semop(udev_lock_semid, &sb, 1) == -1)
		return -1;

	return 0;

}
