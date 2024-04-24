#include "user.h"

int main (int arg, char *argv[]) 
{	
	extern int SO_USERS_NUM, SO_NODES_NUM, SO_BUDGET_INIT, SO_RETRY;
	extern int semafori, memoria_block, mem_stamp, mem_lib, *blocco, fails, budget, indice, pos; 
	extern struct p_details *ptr; 
	extern struct block *ledger; 	

	struct sigaction sa; 
	struct sembuf sops; 
	sigset_t  my_mask;
	
	/* CREO MASCHERA DI SEGNALI */ 
	sa.sa_handler = handle_signal;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);
	sa.sa_mask = my_mask;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);
	
	srand(getpid());
	pos = atoi(argv[1]); /*memorizzo numero node passato da master con cui aggiorno struct p_details */
	
	/* user legge i valori necessari per la sua esecuzione */
	configura_macro();
	
	/* Ottengo tutti i sistemi ipc inizializzati in master.c */
	if ((memoria_block = shmget(SHMID_BLOCK, sizeof(int), IPC_CREAT | 0666)) < 0) {

		fprintf(stderr, "%s: %d. Errore in in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if ((blocco = (int*)shmat(memoria_block, NULL, 0)) == (void *)-1) {
		fprintf(stderr, "%s: %d. Errore in in shmat #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if ((mem_stamp = shmget(getppid(), sizeof(struct p_details)*(SO_USERS_NUM+SO_NODES_NUM), IPC_CREAT | 0666)) < 0) {
		fprintf(stderr, "%s: %d. Errore in in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	if ((ptr = (struct p_details*)shmat(mem_stamp, NULL, 0)) == (void *)-1) {
		fprintf(stderr, "%s: %d. Errore in in shmat #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	ptr[pos].pid = getpid();
	ptr[pos].bilancio_tot = 0;
	ptr[pos].end = 0; 
	
	if ((mem_lib = shmget(SHMID_BOOK, sizeof(struct block)*(SO_REGISTRY_SIZE), IPC_CREAT | 0666)) < 0) {
		fprintf(stderr, "%s: %d. Errore in in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if ((ledger = (struct block*)shmat(mem_lib, NULL, 0)) == (void *)-1) {
		fprintf(stderr, "%s: %d. Errore in in shmat #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if((semafori = semget(getppid(),4,IPC_CREAT | 0600)) < 0) {
	fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}		
	
	budget = SO_BUDGET_INIT;
	fails = 0; 
	indice = 0;

	/* incremento il semaforo 0 del master per risvegliarlo */
	sops.sem_num=0;
	sops.sem_op=1;
	semop(semafori,&sops,1);

	
	/* decremento semaforo per bloccare momentaneamente i processi user */
	sops.sem_num=1;
	sops.sem_op=-1;
	semop(semafori,&sops,1);

	while(fails != SO_RETRY) { /* user rimane in esecuzione fino a quando non ha raggiunto il limite di tentativi falliti - oppure ha ricevuto un segnale */
		
		/* riservo accesso a sezione critica 
		++++ IN VERITA QUESTO SEMAFORO NON E' NECESSARIO IN QUANTO USER EFFETTUA SOLAMENTE UNA LETTURA 
		++++ NELLA MEMORIA CONDIVISA, PERTANTO PUO' ESSERE OMESSO (Stessa cosa per node.c) 
		*/
		sops.sem_num=1;
		sops.sem_op=-1;
		semop(semafori,&sops,1);
		
		user_current_budget();
		
		reset_msg();
		send_msg();
	
		/* rilascio accesso ad altri user */
		sops.sem_num=1;
		sops.sem_op=1;
		semop(semafori,&sops,1);
	}
	
	ptr[pos].end = 1; /* 1 == user ha terminato la sua esecuzione, aggiorno la sua informazione per la print finale in master.c */
	printf("user %d termina\n", getpid());
	exit(EXIT_FAILURE);
}
	
