/*
 * Semaphore.c
 *
 *  Copyright (C) 2008,2010 Stefan Bolus, University of Kiel, Germany
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "Semaphore.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <assert.h>

int Semaphore_new_private (int flags, int val)
{
  int semId = semget (IPC_PRIVATE, 1, flags);
  if (-1 == semId) {
    perror ("semget [Semaphore_new_private]");
    assert (semId >= 0);
  }
  Semaphore_setvalue (semId, val);

  return semId;
}

void Semaphore_wait (int semId)
{
  int ret;
  struct sembuf sops[1];
	
  sops[0].sem_num = 0;
  sops[0].sem_op = -1;
  sops[0].sem_flg = 0;

  do {
	  errno = 0;
	  ret = semop (semId, sops, 1);
	  if (0 != ret) {
		  if (EINTR != errno) {
			  perror ("semop [Semaphore_wait]");
			  exit (1);
		  }
	  }
	  else break;
  } while (TRUE);
}

void Semaphore_post (int semId)
{
  int ret;
  struct sembuf sops[1];

  sops[0].sem_num = 0;
  sops[0].sem_op = 1;
  sops[0].sem_flg = 0;

  ret = semop (semId, sops, 1);
  if (0 != ret) {
    perror ("semop [Semaphore_post]");
    assert (ret == 0);
  }
}

void Semaphore_setvalue (int semId, int val)
{
  int ret;

  {
    union {
      int val;
      struct semid_ds *buf;
      unsigned short *array;
    } arg;
    arg.val = val;
    
    ret = semctl (semId, 0, SETVAL, arg);
  }
  if (0 != ret) {
    perror ("semctl (..., SETVAL, ...)");
    assert (ret == 0);
  }
}

void Semaphore_destroy (int semId)
{
  int ret = semctl (semId, 0, IPC_RMID);
  if (ret < 0) {
    perror ("semctl (..., IPC_RMID)");
    assert (FALSE);
  }
}
