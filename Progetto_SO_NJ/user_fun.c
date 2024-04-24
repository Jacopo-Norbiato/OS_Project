#include "user.h"

int SO_USERS_NUM, SO_NODES_NUM, SO_SIM_SEC, SO_BUDGET_INIT, SO_RETRY, 
	SO_REWARD, SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC;
int semafori, memoria_block, mem_stamp, mem_lib, *blocco, fails, budget, indice, pos, msgqid_att; 
struct tx mcontent; 
struct p_details *ptr; 
struct block *ledger;
struct mymsg txmsg; 
struct sembuf sops; 

int leggi_file(char *configOption, char *path) {
  
  int configValue=0;
  char *rowConfigOption =malloc(sizeof(char)*50);  

  FILE *fp = fopen(path, "r");
  if (fp == NULL){
    printf("ERRORE nell'apertura del file di configurazione: %s \n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  while (fscanf(fp, "%s %d", rowConfigOption, &configValue) !=EOF){
    if(strcmp(configOption, rowConfigOption)==0){
    fclose(fp);
    free(rowConfigOption);
    return configValue;
    }
  }
  fclose(fp);
  free(rowConfigOption);
  return -1;

}

void configura_macro() {
    
  SO_USERS_NUM = leggi_file("users", CONFIGPATH);
	SO_NODES_NUM = leggi_file("nodes", CONFIGPATH);
  SO_SIM_SEC = leggi_file("max_time", CONFIGPATH);
	SO_BUDGET_INIT = leggi_file("budget", CONFIGPATH);
	SO_RETRY = leggi_file("attempts", CONFIGPATH);
	SO_REWARD = leggi_file("reward", CONFIGPATH);
	SO_MAX_TRANS_GEN_NSEC = leggi_file("max_time_gen", CONFIGPATH);
  SO_MIN_TRANS_GEN_NSEC = leggi_file("min_time_gen", CONFIGPATH);

}

void user_current_budget() {
	
	int j = 0, i; 
	if(*blocco != 0) {  
		while(j < *(blocco)) {
			for(i=0;i<SO_BLOCK_SIZE-1;i++) {

				if(ledger[j].book[i].receiver == getpid()) { /* cerco corrispondenza tramite il pid */
					budget += ledger[j].book[i].quantity; /* aggiorno al bilancio proprio la quantita ricevuta da un altro user */
				}
			}
		j++; /* mi sposto nel prossimo block incrementando la variabile j che viene associata al pointer ledger */
		}
	}	
}

void user_random_pick() {

	int x = rand()%SO_USERS_NUM; 
	while(ptr[x].pid == getpid() || ptr[x].end == 1) { /* verifico che user estratto non sia corrispondente e non sia terminato */
		x = rand()%SO_USERS_NUM; 
	}
	
	mcontent.receiver = ptr[x].pid; /* inserisco il pid trovato nella struct txmsg (tramite mcontent) da inviare ai node */
}

void user_quantity() {
	
	int quantita;
	quantita = rand()%budget+2;

	if(((SO_REWARD * quantita)/100) >= 1) {
	mcontent.reward = (SO_REWARD*quantita)/100; 
	} else {
	mcontent.reward = 1; /* arrotondo per difetto ad 1 e salvo questo valore in mcontent che passa poi a txmsg.mtext */  
	}

	mcontent.quantity = quantita - mcontent.reward; /* quantita che viene inviata ad altro user */ 
}

void reset_msg() {
		
		mcontent.sender = getpid();
   	mcontent.receiver = 0;
    mcontent.quantity = 0;
		mcontent.reward = 0;  
		mcontent.timestamp = 0; 
}

void get_timestamp() {
	struct timespec time; /* inizializzo una struttura timespec per ottenere informazioni sul tempo */
	int x; 

	if((x = clock_gettime(CLOCK_REALTIME, &time)) == -1) {
	fprintf(stderr, "%s : %d. Errore in in gettime() user #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
	exit(EXIT_FAILURE);
	}
	/* timestamp = secondi attuali (in nanosec) + nanosec nel momento attuale */
		mcontent.timestamp = time.tv_sec * (NANOSEC) + time.tv_nsec; 
}

void send_msg() {
	
	int node_receiver; 

	if(budget>=2) {
	
		node_receiver = ptr[rand()%SO_NODES_NUM+SO_USERS_NUM].pid; 	
		user_random_pick();
    get_timestamp();
    user_quantity(); 
		
		if((msgqid_att = msgget(node_receiver, 0)) == -1) { /* indirizzo coda messaggi da agganciare da parte del processo node */
			fprintf(stderr, "%s: %d. Errore in in msgget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		txmsg.mtype = 1; /* così che tipo messaggio e' > 0 */
    txmsg.mtext = mcontent; /* passo la transazione individuata come contenuto mtext del messaggio da inviare a node */

		if (msgsnd(msgqid_att, &txmsg, sizeof(struct tx), IPC_NOWAIT) == -1) { /* Msgsnd fallita */
			fprintf(stderr, "%s: %d. Errore in in msgsnd #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
			exit(EXIT_FAILURE);
		} else { /* Msgsnd riuscita */
			budget -= 	mcontent.quantity; /* aggiorno il budget con la quantita inviata ad altro user */
			fails = 0; /* i tentativi falliti devono essere consecutivi, quindi resetto il contatore fails */
			waiting_trans_gen();
		}

	} else {
		fails ++;
	}

}

void waiting_trans_gen() {
		
	struct timespec delta; 
	
   	delta.tv_sec = 0; 
  	delta.tv_nsec = rand()%SO_MAX_TRANS_GEN_NSEC + SO_MIN_TRANS_GEN_NSEC; 
  	nanosleep(&delta, &delta); 	
  		
}

void handle_signal (int signal) {

	switch(signal){
	
		case SIGINT: /* in caso di SIGINT, user invia lo stesso una transazione a node */
			pause(); /* pause() poiche' user rimane in attesa che master svolga i propri compiti */
			break; 

		case SIGALRM:
			break; 

		case SIGTERM:
    		shmdt(ledger); /* faccio detach delle memorie condivise */
    		shmdt(ptr);
				exit(EXIT_SUCCESS);
    		break;

    	default:
    		break;
   
   	}

}