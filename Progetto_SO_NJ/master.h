#include "header.h"

int leggi_file(char *configOption, char *path);
void configura_macro(); 
void lettura_bilanci(); 
void print_Stats();
void print_user(); 
void print_node(); 
void print_info(struct p_details *ptr);
void attesa_master(); 
void remove_IPC(); 
void handle_signal (int signal);
