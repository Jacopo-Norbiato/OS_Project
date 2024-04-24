#include "master.h"

int SO_USERS_NUM, SO_NODES_NUM, SO_SIM_SEC, SO_BUDGET_INIT; 
int semafori, memoria_block, mem_stamp, mem_lib, *blocco, user_running, user_stopped, msq_stats; 
struct p_details *ptr;
struct tx mcontent;  
struct block *ledger; 
struct msqid_ds buffer; /* struttura associata ad una coda di messaggi */

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
}

void lettura_bilanci() {
	
	int i, j, k; 
	
	for(j=0;j<SO_USERS_NUM;j++) { /* scorro tutti i processi user */
		
		ptr[j].bilancio_tot = SO_BUDGET_INIT; /* assegno budget iniziale al processo user j*/  
		
			for(i=0;i<*blocco;i++) { /* scorro tutti i blocchi scritti (int * blocco) in libro mastro */
			
				for(k=0; k<SO_BLOCK_SIZE-1; k++) {
					if(ptr[j].pid == ledger[i].book[k].sender) { /* corrispondenza tra processo user j e sender in libro mastro */
					
						ptr[j].bilancio_tot	-= 	ledger[i].book[k].quantity; /* decremento il bilancio con i soldi inviati */ 
					}
					if(ptr[j].pid == ledger[i].book[k].receiver) { /* corrispondenza tra processo user j e receiver in libro mastro */

						ptr[j].bilancio_tot	+= 	ledger[i].book[k].quantity; /* incremento il bilancio con i soldi inviati da altri user */ 
					}
    		}
    	}	
    }

    for(j=SO_USERS_NUM;j<SO_USERS_NUM+SO_NODES_NUM;j++) { /* scorro tutti i processi node dalla posizione j=SO_USERS_NUM */
		
		ptr[j].bilancio_tot = 0; /* assegno 0 al budget iniziale al processo node j*/  

    	for(i=0;i<*blocco;i++) {			
			if(ptr[j].pid == ledger[i].book[SO_BLOCK_SIZE-1].receiver) { /* cerco corrispondenza tra node j e il receiver in libro mastro in ultima posizione del blocco */
			
				ptr[j].bilancio_tot += ledger[i].book[SO_BLOCK_SIZE-1].quantity; 
			}
		}
	}
}

void print_user() { /* funzione che effettua un sort dei due array coinvolti, quindi effettua stampa dei valori min e max */

	int i, j, temp;
	long res;
	/* inizializzo due array ausiliari con memoria dinamica */
	long * pid_user = (long *)malloc(sizeof(long)*SO_USERS_NUM);
	int * budget_user = (int *)malloc(sizeof(int)*SO_USERS_NUM);
	
	/* copio i valori contenuti in struct p_details nei due array */
	for(i = 0; i < SO_USERS_NUM; i++) {
		*(pid_user+i) = ptr[i].pid; 
		*(budget_user+i) = ptr[i].bilancio_tot; 
	}

	/* effettuo un ordinamento dei due array */
	for(i = 0; i < SO_USERS_NUM; i++) {
		for(j = i+1; j<SO_USERS_NUM; j++) {

			if(*(budget_user+i)<*(budget_user+j)) {
			
				/* trasferisco i valori del bilancio utilizzando temp */
				temp = *(budget_user+i);
				*(budget_user+i) = *(budget_user+j);
				*(budget_user+j) = temp; 
				
				/* eseguo la stessa operazione per pid processi user */
				res = *(pid_user+i);
				*(pid_user+i) = *(pid_user+j);
				*(pid_user+j) = res; 		
			}
		}
	}
	
	/* stampo i 3 user con bilancio minimo */
	for(i = 0; i < 4; i++) {
		printf("- USER pid : %ld    bilancio MAX : %d\n", *(pid_user+i), *(budget_user+i));
	}
	
	/* stampo i 3 user con bilancio massimo */
	for(i = SO_USERS_NUM-1; i > SO_USERS_NUM-5; i--) {
		printf("- USER pid : %ld    bilancio MIN : %d\n", *(pid_user+i), *(budget_user+i));
	}
	
	printf("\n");
	/* rimuovo la memoria allocata prima di terminare la funzione */
	free(pid_user);
	free(budget_user);
}

/* stampo il processo node con bilancio massimo e quello con bilancio minimo */
void print_node() {
	
	int i, p_min, p_max, minimo, maximo;
	
	/* SO_USERS_NUM = posizione 0 in struct p_details dei processi node */
	minimo = ptr[SO_USERS_NUM].bilancio_tot; 
	p_min = SO_USERS_NUM; 
	maximo = ptr[SO_USERS_NUM].bilancio_tot; 
	p_max = SO_USERS_NUM; 

	for(i = SO_USERS_NUM+1; i < (SO_USERS_NUM+SO_NODES_NUM); i++) {
		
		if(ptr[i].bilancio_tot < minimo) {
			p_min = i; 
			minimo = ptr[i].bilancio_tot; 
		}
		if(ptr[i].bilancio_tot > maximo) {
			p_max = i; 
			maximo = ptr[i].bilancio_tot; 
		}
	}
	printf("- NODE pid : %d    bilancio MAX : %d\n", ptr[p_max].pid, maximo); 
	printf("- NODE pid : %d    bilancio MIN : %d\n", ptr[p_min].pid, minimo); 
	
}

void print_info(struct p_details *ptr) {
	
	/* il riepilogo sui processi consiste nella stampa di n = 10 processi tra user e node */

	int i;
	lettura_bilanci(); 
	
	if(user_running>=10) {  /* se ho ancora user in esecuzione, allora faccio una selezione nella print_Stats() */  
		if(SO_NODES_NUM >= 10) {
			
			print_user(); /* stampa i 3 min e 3 max user */
			print_node(); /* stampa i 2 min e 2 max node */

		} else {
		
			print_user(); 
			for(i = SO_USERS_NUM; i < SO_USERS_NUM+SO_NODES_NUM; i++) {
				printf("Node : %d    bilancio : %d\n", ptr[i].pid, ptr[i].bilancio_tot);
			}
		} 
	} else {
		
		if(SO_NODES_NUM>=10) { /* i node in esecuzione sono maggiori degli user */
			
			for(i = 0; i < SO_USERS_NUM; i++) { /* stampo tutti gli user */
				if(ptr[i].end == 1) {
					printf("User [terminato] : %d    bilancio : %d\n", ptr[i].pid, ptr[i].bilancio_tot);
				} else {
					printf("User [in esecuzione] : %d    bilancio : %d\n", ptr[i].pid, ptr[i].bilancio_tot);	
				}
			}
			print_node(); 

		} else {
			
			for(i = 0; i < SO_USERS_NUM; i++) { /* stampo tutti gli user */ 
				if(ptr[i].end == 1) {
					printf("User [terminato] : %d    bilancio : %d\n", ptr[i].pid, ptr[i].bilancio_tot);
				} else {
					printf("User [in esecuzione] : %d    bilancio : %d\n", ptr[i].pid, ptr[i].bilancio_tot);	
				}
			}
			for(i = SO_USERS_NUM; i < SO_USERS_NUM+SO_NODES_NUM; i++) { /* stampo tutti i node */
				printf("Node : %d    bilancio : %d\n", ptr[i].pid, ptr[i].bilancio_tot);
			}
		}
	}
}

void print_Stats() {
	
	int i; 
	printf("\n\t\t TERMINAZIONE \n");
	print_info(ptr);
	printf("Processi User terminati : %d\n", user_stopped);

	if(*blocco == SO_REGISTRY_SIZE) {
		printf("******  Raggiunta massima capienza del Libro Mastro ******\n");
	} else {
		printf("****** Sono stati trascritti %d/%d blocchi del Libro Mastro ******\n", *blocco, SO_REGISTRY_SIZE);
	}

	printf("\n\n****** Numero di transazioni rimaste nalla Transaction Pool: ******\n\n"); /* Lista di transazioni non ancora processate */

	for (i = SO_USERS_NUM; i < SO_USERS_NUM + SO_NODES_NUM; i++) { /* Cicla tutti i node */
		
		/* Aggancio msq_stats alla coda di messaggi del processo ptr[i].pid */
		if ((msq_stats = msgget(ptr[i].pid, 0)) == -1) { 
			fprintf(stderr, "%s: %d. Errore in msgget in print_Stats() #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		}
		
		/* Sfrutto la struttura associata msqid_ds al buffer per contare transazioni rimanenti al nodo j */
		if ((msgctl(msq_stats, IPC_STAT, &buffer)) == -1) { 
			fprintf(stderr, "%s: %d. Errore in msgctl in print_Stats() #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		}
		
		printf("[ Node ] :  %d    [ Transazioni non processate ] : %ld \n", ptr[i].pid, buffer.msg_qnum); 
	}

	printf("\n\n");
}


void attesa_master() {
	
	struct timespec delta;
	delta.tv_sec = 1 / NANOSEC;
	delta.tv_nsec = 999999999;
	clock_nanosleep(CLOCK_MONOTONIC, 0, &delta, NULL); 	
}

void remove_IPC() {
	
	/* rimuovo semafori condivisi */
 	semctl (semafori,0,IPC_RMID);
 	
 	/* rimuovo memorie condivise */
	shmdt(ptr);
	shmctl(mem_stamp, IPC_RMID, NULL);
	shmctl(mem_lib, IPC_RMID, NULL);
	shmctl(memoria_block, IPC_RMID, NULL);
	
}

void handle_signal (int signal) {

	switch(signal){

    	case SIGTERM: /* fine simulazione, aspetto morte di tutti i processi figli con waitpid()*/
    		waitpid(0, NULL, 0);
    		remove_IPC();
    		exit(EXIT_SUCCESS);
    		break;

		case SIGINT:	
					printf("SEGNALE RICEVUTO ** SIGINT **, MASTER TERMINA\n\n");
					print_Stats(); 
    			kill(0, SIGTERM); 
    			break; 

		case SIGALRM:
				printf("SEGNALE RICEVUTO ** SIGALRM **, MASTER TERMINA\n\n");
				print_Stats(); 
   			kill(0, SIGTERM);
   			break; 

   		case SIGUSR1:
   				printf("SEGNALE RICEVUTO ** SIGUSR1 **, MASTER TERMINA\n\n");
					print_Stats();     
    			kill(0, SIGTERM); 
    			break; 

    	default:
    		break;
   
   	}
}


