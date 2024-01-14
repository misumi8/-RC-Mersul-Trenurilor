#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#define ANSWR_SIZE 4096
extern int errno;

void herror(const char *str){
    perror(str);
    exit(errno);
}

int port;

int main(int argc, char *argv[]){
    int sd;			
    struct sockaddr_in server;	
    char comanda[100];
    char raspuns[ANSWR_SIZE];

    if (argc != 4){
        printf("Sintaxa: %s <adresa_server> <port> <comanda>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        herror("Eroare la socket().\n");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);  /* adresa IP a serverului | inet_addr = convert to IPv4*/
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr*) &server, sizeof(struct sockaddr)) == -1){
        herror("Eroare la connect().\n");
    }

    bzero(comanda, 100);
    bzero(raspuns, ANSWR_SIZE);
    
    printf("\nStația curentă: ");
    fflush(stdout);
    //read(0, comanda, 100);  
    printf("\nBună, comenzile disponibile sunt:\n1. \"time\" pentru a afla timpul curent\n2. \"mersul_trenurilor\" pentru a vedea rutele disponibile. \n3. \"plecari_in_ora\" pentru a afla detalii despre plecări în următoarea oră. \n4. \"sosiri_in_ora\" pentru a afla detalii despre sosiri în următoarea oră.\n5. \"intarzieri\" pentru a vedea lista trenurilor ce vor întârzia\n6. \"tren_id minute întârziere\" pentru a raporta o întrziere a trenului\n7. \"exit\" pentru a ieși din aplicație\n");
    fflush(stdout);
    
    if(write(sd, "Iasi0", 100) <= 0){
            herror("Eroare la write() spre server.\n");
    }
    while(1){
        printf("1cmd\n");
        fflush(stdout);
        bzero(comanda, 100);
        //read(0, comanda, 100); 
        strcpy(comanda, "M2 1 intarziere\n");
        //sleep(1);
        if(strstr(comanda, "exit") != NULL || strcmp(comanda, "7\n") == 0) break;
        if (write(sd, comanda, 100) <= 0){
            herror("Eroare la write() spre server.\n");
        }
        /* citirea raspunsului dat de server (apel blocant pana cand serverul raspunde) */
        int answer_size = 0;
        if ((answer_size = read(sd, raspuns, ANSWR_SIZE)) < 0){
            herror("Eroare la read() de la server sau serverul a fost închis\n");
        }
        printf ("%s \n", raspuns);
        bzero(raspuns, ANSWR_SIZE);
    }

    close (sd);
}