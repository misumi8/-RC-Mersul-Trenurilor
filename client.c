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

    if (argc != 3){
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
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
    read(0, comanda, 100);  
    printf("\nBună, comenzile disponibile sunt:\n0. \"time\" pentru a afla timpul curent\n1. \"mersul_trenurilor\" pentru a vedea rutele disponibile. \n2. \"plecari_in_ora\" pentru a afla detalii despre plecări în următoarea oră. \n3. \"sosiri_in_ora\" pentru a afla detalii despre sosiri în următoarea oră.\n4. \"intarzieri\" pentru a vedea lista trenurilor ce vor întârzia\n5. \"tren_id minute intarziere\" pentru a raporta o intarziere a trenului\n\n");
    fflush(stdout);
    
    if (write(sd, comanda, 100) <= 0){
            herror("Eroare la write() spre server.\n");
    }
    while(1){
        bzero(comanda, 100);
        read(0, comanda, 100);  
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