#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <math.h>

#define PORT 2024
#define s 4096
#define MAX_CL 3
extern int errno;

float server_timef = 0.0;
char timp_actual[50];

void addStationsDelay(xmlNodePtr train_stations, char delay[4]){
	xmlNodePtr statie = train_stations->children;
	while(statie != NULL){
		if(xmlStrcmp(statie->name, (const xmlChar*)"arrival_delay") == 0){
			int old_delay = atoi(xmlNodeGetContent(statie));
			int new_delay = old_delay + atoi(delay);
			char new_strdelay[4];
			sprintf(new_strdelay, "%d", new_delay);
			xmlNodeSetContent(statie, (const xmlChar*)new_strdelay);
		}
		else if(xmlStrcmp(statie->name, (const xmlChar*)"arrival_time") == 0){
			int ore_intarziere = atoi(delay) / 60;
			int minute_intarziere = atoi(delay) - ore_intarziere * 60;
			char old_delay[6];
			strcpy(old_delay, xmlNodeGetContent(statie));
			//printf("|%s|\n", old_delay);
			char hours_delay[3], minutes_delay[3];
			for(int i = 0; i < 2; ++i){
				hours_delay[i] = old_delay[i];
				minutes_delay[i] = old_delay[i+3];
			}
			hours_delay[2] = '\0';
			minutes_delay[2] = '\0';
			int new_hours_delay = atoi(hours_delay) + ore_intarziere;
			int new_minutes_delay = atoi(minutes_delay) + minute_intarziere;
			if(new_minutes_delay >= 60){
				new_hours_delay++;
                new_minutes_delay -= 60;
			}
			if(new_hours_delay >= 24) new_hours_delay -= 24;
			//printf("|%d|%d|\n", new_hours_delay, new_minutes_delay);
			//fflush(stdout);
			if(new_hours_delay > 9) sprintf(hours_delay, "%d", new_hours_delay);
			else sprintf(hours_delay, "0%d", new_hours_delay);
			if(new_minutes_delay > 9) sprintf(minutes_delay, "%d", new_minutes_delay);
			else sprintf(minutes_delay, "0%d", new_minutes_delay);
			printf("|%s|:|%s|\n", hours_delay, minutes_delay);
			for(int i = 0; i < 2; ++i){
				old_delay[i] = hours_delay[i];
				old_delay[i+3] = minutes_delay[i];
			}
			xmlNodeSetContent(statie, (const xmlChar*)old_delay);
		}
		statie = statie->next;
	}
	if(train_stations->next != NULL) addStationsDelay(train_stations->next, delay);
}

void* new_client(void* arg){
	int csd = *((int*)arg);
	char raspuns[s];
	char comanda[100];
	bzero(&comanda, 100);
	if(read(csd, comanda, 100) <= 0){
		perror("Eroare la read din client sau clientul s-a deconectat\n");
	}
	char statie_client[100];
	strcpy(statie_client, comanda);
	statie_client[strlen(statie_client) - 1] = '\0';
	//printf("\n|%s|\n", statie_client);
	while(1){
		bzero(&comanda, 100);
		bzero(&raspuns, s);
		if(read(csd, comanda, 100) <= 0){
			perror("Eroare la read din client sau clientul s-a deconectat\n");
			break;
		}
		printf("Comanda primita: %s\n", comanda);
		// pregatim raspunsul pentru comanda
		if(strstr(comanda, "time") != NULL || strcmp(comanda, "0\n") == 0){
			strcat(raspuns, "[RASPUNS] Time: ");
			strcat(raspuns, timp_actual);
			strcat(raspuns, "\n");
		}
		else if(strstr(comanda, "mersul_trenurilor") != NULL || strcmp(comanda, "1\n") == 0) {
			strcat(raspuns, "[RASPUNS] Mersul trenurilor:\n\n");
			xmlDocPtr schedule;
			xmlNodePtr root, trenuri;
			schedule = xmlReadFile("schedule.xml", NULL, 0);
			if(schedule == NULL){
				perror("Eroare la citirea fisierului schedule.xml\n");
			}
			root = xmlDocGetRootElement(schedule);
			if(root == NULL){
				perror("Fisierul xml e gol\n");
            }
			trenuri = root->children;
			while (trenuri != NULL) {
				//if (!(trenuri->name == (const xmlChar *)"train")) {
					xmlNodePtr temp = trenuri->children;
					int station_nr = 1;
					while (temp != NULL) {
						if (temp->type == XML_ELEMENT_NODE) {
							//printf("%s: %s\n", temp->name, xmlNodeGetContent(temp));
							if(xmlStrcmp(temp->name, (const xmlChar *)"id") == 0){
								strcat(raspuns, "<=== Trenul ");
								strcat(raspuns, xmlNodeGetContent(temp));
								strcat(raspuns, " ===>\n");
							}
							else if(xmlStrcmp(temp->name, (const xmlChar *)"from") == 0){
								strcat(raspuns, xmlNodeGetContent(temp));
								strcat(raspuns, " -> ");
							}
							else if(xmlStrcmp(temp->name, (const xmlChar *)"to") == 0){
								strcat(raspuns, xmlNodeGetContent(temp));
								strcat(raspuns, "\n");
							}
							else if(xmlStrcmp(temp->name, (const xmlChar *)"new_departure") == 0){
								strcat(raspuns, "Plecarea: ");
								strcat(raspuns, xmlNodeGetContent(temp));
								strcat(raspuns, "\n");
							}
							else if(xmlStrcmp(temp->name, (const xmlChar *)"new_arrival") == 0){
								strcat(raspuns, "Sosirea: ");
								strcat(raspuns, xmlNodeGetContent(temp));
								strcat(raspuns, "\n\n");
							}
							else if(temp->type == XML_ELEMENT_NODE && xmlStrcmp(temp->name, (const xmlChar *)"statie") == 0){
								xmlNodePtr statie = temp->children;
								char nume_statie[100];
								char ord_statie[100];
								//printf("%s | %s\n", temp->name, statie->name);
								while(statie != NULL){
									if(xmlStrcmp(statie->name, (const xmlChar *)"name") == 0){
										strcat(raspuns, "*Statia ");
										sprintf(ord_statie, "%d", station_nr);
										strcat(raspuns, ord_statie);
										strcat(raspuns, ": ");
										strcat(raspuns, xmlNodeGetContent(statie));
										strcat(raspuns, "\n");
										strcat(nume_statie, xmlNodeGetContent(statie));
										station_nr++;
									}
									else if(xmlStrcmp(statie->name, (const xmlChar *)"arrival_time") == 0){
										strcat(raspuns, "Ajunge la ");
										strcat(raspuns, xmlNodeGetContent(statie));
										strcat(raspuns, "\n");
									}
									else if(xmlStrcmp(statie->name, (const xmlChar *)"arrival_delay") == 0){
										strcat(raspuns, "Intarziere: ");
										if(strcmp(xmlNodeGetContent(statie), "0") == 0) strcat(raspuns, "NU\n");
										else {
											strcat(raspuns, xmlNodeGetContent(statie));
											strcat(raspuns, " minute\n");
										}
									}
									statie = statie->next;
								}
								strcat(raspuns, "\n");
							}
						}
						temp = temp->next;
					}
				//}
				trenuri = trenuri->next;
			}
		}
		else if(strstr(comanda, "plecari_in_ora") != NULL || strcmp(comanda, "2\n") == 0){
			strcat(raspuns, "[RASPUNS] Plecări în următoarea oră din stația ");
			strcat(raspuns, statie_client);
			strcat(raspuns, ":\n");
		} 
		else if(strstr(comanda, "sosiri_in_ora") != NULL || strcmp(comanda, "3\n") == 0){
			strcat(raspuns, "[RASPUNS] Sosiri în următoarea oră din stația ");
			strcat(raspuns, statie_client);
			strcat(raspuns, ":\n");
		} 
		else if(strstr(comanda, "intarzieri") != NULL || strcmp(comanda, "4\n") == 0){
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

				xmlDocPtr schedule;
				xmlNodePtr root, trenuri;
				schedule = xmlReadFile("schedule.xml", NULL, 0);
				if(schedule == NULL){
					perror("Eroare la citirea fisierului schedule.xml\n");
				}
				root = xmlDocGetRootElement(schedule);
				//printf("|%s|", root->name);
				//fflush(stdout);
				if(root == NULL){
					perror("Fisierul xml e gol\n");
				}
				trenuri = root->children;
				bool found = false, train_found = false, station_found = false;
				while(trenuri != NULL){
					xmlNodePtr temp = trenuri->children;
					while(temp != NULL){
						if(xmlStrcmp(temp->name, (const xmlChar *)"id") == 0){
							if(strcmp(xmlNodeGetContent(temp), train_id) == 0){
								//Am gasit trenul
								train_found = true;
								while(temp != NULL){
									//cautam daca exista statia clientului in lista statiilor trenului
									if(xmlStrcmp(temp->name, (const xmlChar *)"statie") == 0){
										xmlNodePtr statie = temp->children;
										while(statie != NULL){
											if(xmlStrcmp(statie->name, (const xmlChar *)"name") == 0){
												if(strcmp(xmlNodeGetContent(statie), statie_client) == 0){
													station_found = true;
													char arrival_time[6], ariv_ore[3], ariv_minute[3];
													while(statie != NULL){
														if(xmlStrcmp(statie->name, (const xmlChar *)"arrival_time") == 0){
															strcpy(arrival_time, xmlNodeGetContent(statie));
															arrival_time[5] = '\0';
															for(int i = 0; i < 2; ++i){
																ariv_ore[i] = arrival_time[i];
																ariv_minute[i] = arrival_time[i+3];
															}
															ariv_ore[2] = '\0';
															ariv_minute[2] = '\0';
														}
														statie = statie->next;
													}
													char a_ore[3], a_minute[3];
													for(int i = 0; i < 2; ++i){
														a_ore[i] = timp_actual[i];
														a_minute[i] = timp_actual[i+3];
													}
													a_ore[2] = '\0';
													a_minute[2] = '\0';
													int ora_actual = atoi(a_ore);
													int minute_actual = atoi(a_minute);
													if(ora_actual > atoi(ariv_ore) || (ora_actual >= atoi(ariv_ore) && minute_actual >=  atoi(ariv_minute))){
														found = true;
														addStationsDelay(temp, tdelay);
														break;
													}
													else{
														strcat(raspuns, "[EROARE] Prea devreme pentru sosirea trenului ");
														strcat(raspuns, train_id);
														strcat(raspuns, " la statia ");
														strcat(raspuns, statie_client);
														strcat(raspuns, ".\n");
														break;
													}
												}
											}
											statie = statie->next;
										}
									}
									temp = temp->next;
								}
							}
							if(xmlSaveFile("schedule.xml", schedule) == -1){
								perror("Eroare la salvare in fisier xml\n");
							}
							break;
						}
						temp = temp->next;
					}
					trenuri = trenuri->next;
				}
				if(found == true){
					strcat(raspuns, "[RASPUNS] Raportare înregistrată. Trenul ");
					strcat(raspuns, train_id);
					strcat(raspuns, " a întârziat cu ");
					strcat(raspuns, tdelay);
					strcat(raspuns, " minute.\n");
				}
				else if(train_found == false){
					strcat(raspuns, "[EROARE] Trenul ");
					strcat(raspuns, train_id);
					strcat(raspuns, " nu a fost găsit.\n");
				}
				else if(station_found == false){
					strcat(raspuns, "[EROARE] Stația ");
					strcat(raspuns, statie_client);
					strcat(raspuns, " nu a fost găsită în ruta trenului ");
					strcat(raspuns, train_id);
					strcat(raspuns, ".\n");
				}
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

void* time_simulation(void* arg){
	float minute = 1.0/60.0;
	while(1){
		server_timef += minute;
		usleep(300000);
		if(server_timef >= 24.0) server_timef = 0.0;
		int hour = (int)(server_timef);
		int minutes = (server_timef - (float)hour) * 60;
		//printf("%d\n", minutes);
		if(hour < 10) {
			if(minutes < 10) sprintf(timp_actual, "0%d:0%d", hour, minutes);
			else sprintf(timp_actual, "0%d:%d", hour, minutes);
		} 
		else {
			if(minutes < 10) sprintf(timp_actual, "%d:0%d", hour, minutes);
			else sprintf(timp_actual, "%d:%d", hour, minutes);
		}
	}
}


int main(){
	pthread_t threads[MAX_CL];
	pthread_t ftime;
	int threads_count = 0;

	if(pthread_create(&ftime, NULL, time_simulation, NULL) != 0){
		perror("Eroare la creare thread");
		return errno;
	}

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
		close(sd);
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