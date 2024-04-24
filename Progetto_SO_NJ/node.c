#include "node.h"

int main (int arg, char *argv[]) {

	extern int SO_USERS_NUM, SO_NODES_NUM, SO_TP_SIZE;
	extern int semafori, memoria_block, mem_stamp, mem_lib, *blocco, pos, msgqid_agg, cont_reward, pending_msg; 
	extern struct p_details *ptr; 
	extern struct block *ledger; 	
	extern struct block blocco_attuale; 
	
	struct sigaction sa; 
	struct sembuf sops;
	struct msqid_ds mybuf; 
	struct mymsg txmsg; 
	sigset_t  my_mask;
	
	int i; 

	/* CREO MASCHERA DI SEGNALI */ 
	sa.sa_handler = handle_signal;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);
	sa.sa_mask = my_mask;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	
	srand(getpid());
	pos = atoi(argv[1]); /*memorizzo numero user passato da master con cui aggiorno struct p_details */
	
	/* node legge i valori necessari per la sua esecuzione */
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

	if((semafori = semget(getppid(),0, 0666)) < 0) {
		fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}		
	
	/* creo coda messaggi propria di node */
	if ((msgqid_agg = msgget(getpid(), IPC_CREAT | 0666)) == -1) {
		fprintf(stderr, "%s: %d. Errore in msgget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Copio la struttura di msqid_ds nel buffer puntato da mybuf */
	if ((msgctl(msgqid_agg, IPC_STAT, &mybuf)) == -1) {
		fprintf(stderr, "%s: %d. Errore in msgctl #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Definisco quindi la dimensione byte max del buffer message queue */
	mybuf.msg_qbytes = SO_TP_SIZE * sizeof(struct tx);
	
	/* Imposto dimensione max byte nella message queue */
	if ((msgctl(msgqid_agg, IPC_SET, &mybuf)) == -1) {
		fprintf(stderr, "%s: %d. Errore in msgctl #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* incremento il semaforo 0 del master per risvegliarlo */
	sops.sem_num=0;
	sops.sem_op=1;
	semop(semafori,&sops,1);

	
	/* decremento semaforo per bloccare momentaneamente i processi node */
	sops.sem_num=2;
	sops.sem_op=-1;
	semop(semafori,&sops,1);

	for (;;) {

		cont_reward = 0; 
		
		/* un accesso alla volta in sezione critica */
		sops.sem_num=2;
		sops.sem_op=-1;
		semop(semafori,&sops,1);
	
		if((msgctl(msgqid_agg, IPC_STAT, &mybuf)) == -1) {
			fprintf(stderr, "%s : %d. ERROR in msgctl #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
    	    exit(EXIT_FAILURE); 
    	}

		pending_msg = mybuf.msg_qnum; /* msg_qnum indica il numero di messaggi in coda, definito da struct msqid_ds */
		
		if(pending_msg >= SO_BLOCK_SIZE - 1) { 		
			
			/* La tpool contiene abbastanza messaggi inviati dagli user per creare un blocco candidato con dimensione 
			SO_BLOCK_SIZE-1 siccome ultima transazione e' riservata al processo node  */
	
			for(i = 0; i < SO_BLOCK_SIZE - 1; i++) { /* effettuo msgrcv() per SO_BLOCK_SIZE-1 volte e trascrivo nella struttura struct block blocco_attuale */
            	
            	/* mtype = 0 -> ottengo primo messaggio in coda e lo passo al processo chiamante */
        	    if(msgrcv(msgqid_agg, &txmsg, sizeof(struct tx), 0, 0) == -1) { /* procedo a selezionare un blocco dalla tpool che viene trascritto in libro mastro */
	                fprintf(stderr, "%s : %d. ERROR in msgrcv #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
    	            exit(EXIT_FAILURE); 
            	}   
            	cont_reward += txmsg.mtext.reward; /* aggiorno budget di node */
            	blocco_attuale.book[i] = txmsg.mtext; /* copio mtxet trovato da msgrcv() e trascrivo il blocco attuale alla posizione i */ 
			}
			
			process_last_tx(); /* aggiungo quindi ultima transazione che si caratterizza per la macro definita in header.h */
			
			/* Decremento per riservare accesso a libro mastro in memoria condivisa con semaforo binario 3 */
			sops.sem_num=3; 
			sops.sem_op=-1; 
			semop(semafori,&sops,1);
			
			if(*blocco <= SO_REGISTRY_SIZE-1) { /* Il libro mastro non è ancora completo */
			
				blocco_attuale.index = *blocco;  /* conservo indice attuale dei blocchi trascritti */
				ledger[*blocco] = blocco_attuale; /* trascrivo in libro mastro il blocco appena generato */
				*blocco += 1; /* aggiorno il contatore dei blocchi trascritti */
				process_wait(); /* simulazione attesa scrittura in libro mastro */
				
			} else { /* termino poiche' *blocco == SO_REGISTRY_SIZE */
			
				kill(getppid(), SIGUSR1); /* avviso il padre che libro mastro e' totalmente scritto */ 
				pause(); /* metto in pausa in attesa che master esegua i suoi compiti */
			}
		
			/* Incremento per consentire l'esecuzione ad altri processi node */		
			sops.sem_num=3; 
			sops.sem_op=1; 
			semop(semafori,&sops,1);
		}
 		/* se pending_msq non e' grande abbastanza, allora node torna nel ciclo infinito for(;;)
		fino a quando non ha le informazioni necessarie */ 
		
		/* rilascio risorsa sem_num 2 così che altri node possano accedere in sezione critica */
		sops.sem_num=2;
		sops.sem_op=1;
		semop(semafori,&sops,1);
	}
}
