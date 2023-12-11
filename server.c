#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define PORT 2024
#define s 4096
#define MAX_CL 3
extern int errno;

void* new_client(void* arg){
	int csd = *((int*)arg);
	char raspuns[s];
	char comanda[100];
	while(1){
		bzero(&comanda, 100);
		bzero(&raspuns, s);
		if(read(csd, comanda, 100) <= 0){
			perror("Eroare la read din client sau clientul s-a deconectat\n");
			break;
		}
		printf("Comanda primita: %s\n", comanda);
		// pregatim raspunsul pentru comanda
		if(strstr(comanda, "mersul_trenurilor") != NULL){
			strcat(raspuns, "[RASPUNS] Mersul trenurilor:\n");
		}
		else if(strstr(comanda, "plecari_in_ora") != NULL){
			strcat(raspuns, "[RASPUNS] Plecări în următoarea oră:\n");
		} 
		else if(strstr(comanda, "sosiri_in_ora") != NULL){
			strcat(raspuns, "[RASPUNS] Sosiri în următoarea oră:\n");
		} 
		else if(strstr(comanda, "intarzieri") != NULL){
			strcat(raspuns, "[RASPUNS] Întârzieri:\n");
		} 
		else if(strstr(comanda, "intarziere") != NULL){
			if(comanda[2] != ' ' || (comanda[3] < '0' || comanda[3] > '9')) strcat(raspuns, "[EROARE] Sintaxa greșită.\n");
			else{
				char train_id[3];
				bzero(&train_id, 3);
				strncpy(train_id, comanda, 2);
				train_id[2] = '\0';
				char tdelay[4];
				bzero(&tdelay, 4);
				for(int i = 0; i < 3; ++i){
					if(comanda[i+3] >= '0' && comanda[i+3] <= '9') tdelay[i] = comanda[i+3];
				}
				tdelay[3] = '\0';

				strcat(raspuns, "[RASPUNS] Raportare înregistrată. Trenul ");
				strcat(raspuns, train_id);
				strcat(raspuns, " a întârziat cu ");
				strcat(raspuns, tdelay);
				strcat(raspuns, " minute.\n");
			}
		}
		else strcat(raspuns, "[EROARE] Comandă inexistentă.\n");

		if(write(csd, raspuns, s) == -1){
			perror("Eroare write to client");
			break;
		}
	}
	if(close(csd) == -1) perror("Eroare la close.");
	pthread_exit(NULL);
}

int main(){
	pthread_t threads[MAX_CL];
	int threads_count = 0;

	struct stat st;
    if (stat("schedule.xml", &st) == -1){
        perror("stat xml\n");
		return errno;
	}
	const size_t schedule_size = st.st_size;

    struct sockaddr_in server;	
    struct sockaddr_in from;
    int sd;	

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
    	perror ("Eroare la socket().\n");
    	return errno;
    }

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

	//in caz ca adresa deja se foloseste
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1){
    	perror("Eroare la bind().\n");
    	return errno;
    }

    printf("ip: %d \t port: %d\n", server.sin_addr.s_addr, server.sin_port);
    fflush(stdout);

    if (listen(sd, MAX_CL) == -1){
    	perror("Eroare la listen().\n");
    	return errno;
    }

    while(1){
    	int client;
    	int length = sizeof(from);

    	//printf ("Asteptam la portul %d...\n",PORT);
    	//fflush (stdout);

    	/* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    	client = accept(sd, (struct sockaddr *) &from, &length);
    	if(client < 0){
    		perror ("Eroare la accept().\n");
    		continue;
    	}

		if(pthread_create(&threads[threads_count], NULL, new_client, (void*)&client) != 0){
			perror("Eroare la creare thread");
			return errno;
		}
		threads_count++;
		printf("Thread nou\n");

		// daca a fost atins nr max de clienti
		if(threads_count >= MAX_CL){
			threads_count = 0;
			while(threads_count < MAX_CL){
				pthread_join(threads[threads_count], NULL);
				threads_count++;
			}
			threads_count = 0;
		}
    }
	
	close(sd);
}