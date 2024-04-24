#include "node.h"

int SO_USERS_NUM, SO_NODES_NUM, SO_TP_SIZE, SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC;
int semafori, memoria_block, mem_stamp, mem_lib, *blocco, fails, budget, indice, 
    pos, msgqid_agg, cont_reward, pending_msg; 
struct p_details *ptr; 
struct block *ledger;
struct block blocco_attuale;
struct msqid_ds mybuf; 
struct timespec delta; 

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
    SO_TP_SIZE = leggi_file("tp_size", CONFIGPATH);
    SO_MIN_TRANS_PROC_NSEC = leggi_file("min_time_proc", CONFIGPATH);
    SO_MAX_TRANS_PROC_NSEC = leggi_file("max_time_proc", CONFIGPATH);
}

void get_timestamp() {
    struct timespec time; /* inizializzo una struttura timespec per ottenere informazioni sul tempo */
    int x; 

    if((x = clock_gettime(CLOCK_REALTIME, &time)) == -1) {
    fprintf(stderr, "%s : %d. Errore in gettime() node #%03d %s\n", __FILE__, __LINE__, errno, strerror(errno));
    exit(EXIT_FAILURE);
    }
    /* timestamp = secondi attuali (in nanosec) + nanosec nel momento attuale */
    blocco_attuale.book[SO_BLOCK_SIZE-1].timestamp = time.tv_sec * (NANOSEC) + time.tv_nsec; 
}

void process_last_tx() {
        
    /* aggiungo in posizione [last] i dettagli dell'ultima transazione propria del node */
    blocco_attuale.book[SO_BLOCK_SIZE-1].sender = LAST_TX; 
    blocco_attuale.book[SO_BLOCK_SIZE-1].receiver = getpid(); 
    blocco_attuale.book[SO_BLOCK_SIZE-1].quantity = cont_reward; 
    blocco_attuale.book[SO_BLOCK_SIZE-1].reward = 0; 

    ptr[pos].bilancio_tot += cont_reward; 
    get_timestamp();
}

void process_wait() {
    
    int intervallo; 

    intervallo = (rand()%(SO_MAX_TRANS_PROC_NSEC - SO_MIN_TRANS_PROC_NSEC +1)) + SO_MIN_TRANS_PROC_NSEC;
    delta.tv_sec = intervallo/NANOSEC; 
    delta.tv_nsec = intervallo/NANOSEC; 

    clock_nanosleep(CLOCK_MONOTONIC, 0, &delta, NULL); 
}


void handle_signal (int signal) {

    switch(signal){
    
        case SIGINT: /* node si mette in attesa, master esegue i suoi compiti */
            pause();
            break; 

        case SIGTERM:
            msgctl(msgqid_agg, IPC_STAT, &mybuf); /* chiudo la message queue */
            msgctl(msgqid_agg, IPC_RMID, NULL);
            shmdt(ledger); /* faccio detach delle memorie condivise */
            exit(EXIT_SUCCESS);
            break;
        
        case SIGALRM:
            break; 

        default:
            break;
    }
}