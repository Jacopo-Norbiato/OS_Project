#ifndef __HEADER_H__
#define __HEADER_H__
#endif
#define _GNU_SOURCE 

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <string.h>
#include <errno.h>

/* macro per leggere valori contenuti nel file */ 
#define CONFIGPATH "./opt_3.conf"

/* variabili definite precedentemente l'esecuzione */
#define SO_REGISTRY_SIZE	1000
#define SO_BLOCK_SIZE	10

/* macro predefinita per riconoscere ultima transazione in blocco scritta da processo node */
#define LAST_TX -1

/* id macro memorie condivise */  
#define SHMID_BLOCK 321
#define SHMID_BOOK 213

/* id macro semafori */ 
#define SEM_ID  

#define CLOCK_REALTIME  0
#define NANOSEC 1000000000


/* struttura per ottenere informazioni di ciascun processo running */
struct p_details {
  pid_t pid;
  int bilancio_tot; 
  int end; 
};
    
/* struttura transazione */
struct tx   { 
  pid_t sender; 
  pid_t receiver;
  int reward;
  int quantity;
  long timestamp; 
};

/* struttura blocco libro mastro */
struct block{
  int index;
  struct tx book[SO_BLOCK_SIZE]; /*libro mastro equivale ad array di tx, dimensione SO_BLOCK_SIZE */
};

/* struttura messaggio che viene comunicato tra user e node */
struct mymsg {
  long mtype; 
  struct tx mtext; 
};
