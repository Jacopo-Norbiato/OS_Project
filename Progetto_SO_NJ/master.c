#include "master.h"

void reserve(int semnum, int value);
void release(int semnum, int value);

int main() {
	
	extern int SO_USERS_NUM, SO_NODES_NUM, SO_SIM_SEC; 
	extern int semafori, memoria_block, mem_stamp, mem_lib, *blocco, user_running, user_stopped; 
	extern struct p_details *ptr; 
	extern struct block *ledger; 

	struct sigaction sa; 
	struct sembuf sops; 
	
	sigset_t  my_mask;
	
	int i, j, status_p, status_n; 
	char b [sizeof(int)];	
	char *args [2];
	args[0] = (char *)malloc(sizeof(char)*10);
	args[1] = (char *)malloc(sizeof(int));

	/* Ottengo i valori da lettura file esterno */
	configura_macro();
	
	
	/* CREO MASCHERA DI SEGNALI */ 
	sa.sa_handler = handle_signal;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);
	sa.sa_mask = my_mask;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("MASTER PID : %d\n", getpid());
	
	/* Memoria condivisa per contare le pagine del libro mastro */
	if ((memoria_block = shmget(SHMID_BLOCK, sizeof(int), IPC_CREAT | 0666)) < 0) {

		fprintf(stderr, "%s: %d. Errore in in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if((blocco = (int*)shmat(memoria_block, NULL, 0)) == (void *)-1){
		fprintf(stderr, "ERROR shmget");
		exit(EXIT_FAILURE);
	} 
	
	/* Memoria condivisa per informazioni relative ai processi contenute in struct p_details */	
	if ((mem_stamp = shmget(getpid(), sizeof(struct p_details) * (SO_USERS_NUM + SO_NODES_NUM), IPC_CREAT | IPC_EXCL | 0660)) < 0) {
		fprintf(stderr, "%s: %d. Errore in in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if((ptr = (struct p_details*)shmat(mem_stamp, NULL, 0)) == (void *)-1){
		fprintf(stderr, "ERROR shmget");
		exit(EXIT_FAILURE);
	} 

	/* Memoria condivisa per il libro mastro */
	if ((mem_lib = shmget(SHMID_BOOK, sizeof(struct block)*(SO_REGISTRY_SIZE), IPC_CREAT | 0666)) < 0) {

		fprintf(stderr, "%s: %d. Errore in in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	ledger = (struct block*)shmat(mem_lib, NULL, 0); /* ledger e' il pointer che dovra scorrere il Libro Mastro a terminazione */
	
	*blocco = 0;	/* assegno 0 al contatore di blocchi trascritti in libro mastro */ 
	
	if((semafori=semget(getpid(),4,IPC_CREAT | 0666)) < 0) {

		fprintf(stderr, "%s: %d. Errore in in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	semctl (semafori, 0,SETVAL,0); /* semaforo per gestire processo master */
	semctl (semafori, 1,SETVAL,0); /* semaforo per gestire processi user */
	semctl (semafori, 2,SETVAL,0); /* semaforo per gestire processi node */
	semctl (semafori, 3,SETVAL,1); /* semaforo per gestire accesso libro mastro */

	printf("\nCreazione user ... \n");

	for (i = 0; i < SO_USERS_NUM; i++) {

	switch(status_p= fork()) {

	case -1:
		fprintf(stderr, "%s : %d. Errore nell'esecuzione della fork #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
		break;

		case 0:
		sprintf(b, "%d", i);
		args[0] =
		 "./user";
		args[1] = b;
		if(execve("./user", args, NULL) < 0) {
		fprintf(stderr, "%s : %d. Errore in execve(./user) #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
		}
		break; 

		default:
		break;
		}
	}


	/*il gestore attende che tutti USER siano stati creati e torna a lavorare*/
	sops.sem_num=0;
	sops.sem_op=-SO_USERS_NUM;
	semop(semafori,&sops,1);
	
	printf("\nCreazione node ... \n");

	for(j = SO_USERS_NUM; j < (SO_USERS_NUM+SO_NODES_NUM); j++) {
		
		switch(status_n = fork()) {
		
		case -1:
		fprintf(stderr, "%s : %d. Errore nell'esecuzione della fork #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
		break;

		case 0:
		sprintf(b, "%d", j);
		args[0] = "./node";
		args[1] = b;
		if(execve("./node", args, NULL) < 0) {
		fprintf(stderr, "%s : %d. Errore in execve(./node) #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
		}
		break; 

		default:
		break;
			
		}

	}
	
	/*il gestore attende che tutti NODE siano stati creati e procede con esecuzione*/
	sops.sem_num=0;
	sops.sem_op=-SO_NODES_NUM;
	semop(semafori,&sops,1);

	for (i = 0; i <(SO_USERS_NUM+SO_NODES_NUM);i++) {
		
		if (i < SO_USERS_NUM) {
			printf("user : %d\n", ptr[i].pid);
		} else {
			printf("node : %d\n", ptr[i].pid);
		}
	}

	printf("\nAvvio esecuzione ... \n\n");

	/* user ritornano in esecuzione */
	sops.sem_num=1;
	sops.sem_op=SO_USERS_NUM+1; /* +1 perche' voglio che accedano uno alla volta alla sezione critica in user.c */
	semop(semafori,&sops,1);
	
	/* node ritornano in esecuzione */
	sops.sem_num=2;
	sops.sem_op=SO_NODES_NUM+1;
	semop(semafori,&sops,1);
			
	alarm(SO_SIM_SEC);	/* imposto signal alarm-time dell'esecuzione */
	
	user_running = SO_USERS_NUM;
	user_stopped = 0; 

	for(;;) {

		attesa_master(); 
		printf("\n");

		/* controllo nella struct p_details lo stato di esecuzione dei processi user */
		for(i = 0; i < SO_USERS_NUM; i++) {
			if(ptr[i].end == 1) {
				user_stopped++; /* aggiorno il numero di processi user terminati */
			}
		}
		
		user_running = user_running - user_stopped;

		if(user_running <= 1) { /* se rimane un solo processo user, questi non puo piu inviare transazioni ad altri user */
			
			print_Stats(); 		
			kill(0, SIGTERM); /* kill(pid_t pid, int sig) -> se pid == 0, il segnale viene inviato ad ogni processo del chiamante */
			exit(EXIT_SUCCESS);
			
		}

	print_info(ptr);  	
 	printf("\n*** User teminati : %d\t User in esecuzione : %d\n*** Blocchi scritti in Libro Mastro : %d\n\n", user_stopped, user_running, *blocco);
 	
 	}
	free(args[0]);
	free(args[1]);	 
	return 0; 	
}
