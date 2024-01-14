#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <math.h>
#include <semaphore.h>

#define PORT 2025
#define s 4096
#define MAX_CL 5
#define TRENUL_ASTEAPTA 10
extern int errno;

int clienti[MAX_CL];
float server_timef = 1.0;
char timp_actual[50];
//sem_t sem;
pthread_mutex_t mutex[4];

struct Statie{
	char delay[4];
	char arr_time[6]; // xx:xx
	char stat_name[50];
};

struct Tren{
	char id[3];
	char dep_time[6];
	char arr_time[6];
	char from[50];
	char to[50];
	struct Statie* ruta; 
	int no_of_stations;
};

int no_trains;
struct Tren* trainsInfo;

void reset_schedule();

void sentAnuntIntarziere(int sd, char* trainId, char* intarziere, char* statie){
	char anunt[200];
	bzero(anunt, sizeof(anunt));
	strcat(anunt, "[ANUNȚ] Trenul ");
	strcat(anunt, trainId);
	strcat(anunt, " va intârzia cu ");
	strcat(anunt, intarziere);
	strcat(anunt, " minute începând cu stația ");
	strcat(anunt, statie);
	anunt[strlen(anunt)] = '\0';
	for(int i = 0; i < MAX_CL; ++i){
		int d = clienti[i];
		//printf("%d = %d | sd: %d\n", i, d, sd);
		if(d != sd /*&& (cd != 0 || cd != -1)*/){
			if(write(d, anunt, strlen(anunt)) == -1){
				perror("Eroare la write anunt");
				//exit(125);
			}
		}
	}
}

int getNoOfTrains(xmlNodePtr root){
	xmlNodePtr train = root->children;
	int count = 0;
	while(train != NULL){
		if(xmlStrcmp(train->name, (const xmlChar*)"train") == 0) count++;
        train = train->next;
	}
	return count;
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
		//bool working = false;
		int stationsNo = 0;
		bool primaIntarziere = false;
		bzero(&comanda, 100);
		bzero(&raspuns, s);
		if(read(csd, comanda, 100) <= 0){
			perror("Eroare la read din client sau clientul s-a deconectat\n");
			break;
		}
		printf("Comanda primita: %s\n", comanda);
		// pregatim raspunsul pentru comanda
		if(strstr(comanda, "<admin>reset_schedule") != NULL || strcmp(comanda, "8668\n") == 0){
			server_timef = 0.0;
			for(int k = 0; k < 4; ++k) pthread_mutex_lock(&mutex[k]);
			reset_schedule();
			for(int k = 0; k < 4; ++k) pthread_mutex_unlock(&mutex[k]);
		}
		else if(strstr(comanda, "change_station") != NULL){
			strcpy(statie_client, &comanda[15]);
			statie_client[strlen(statie_client) - 1] = '\0';
		}
		else if(strstr(comanda, "time") != NULL || strcmp(comanda, "1\n") == 0){
			strcat(raspuns, "[RASPUNS] Time: ");
			strcat(raspuns, timp_actual);
			strcat(raspuns, "\n");
		}
		else if(strstr(comanda, "mersul_trenurilor") != NULL || strcmp(comanda, "2\n") == 0) {
			//working = true;
			strcat(raspuns, "[RASPUNS] Mersul trenurilor:\n\n");
			pthread_mutex_lock(&mutex[0]);
			for(int i = 0; i < no_trains; ++i){
				strcat(raspuns, "\n<=== Trenul ");
				strcat(raspuns, trainsInfo[i].id);
				strcat(raspuns, " ===>\n");
				strcat(raspuns, trainsInfo[i].from);
				strcat(raspuns, " -> ");
				strcat(raspuns, trainsInfo[i].to);
				strcat(raspuns, "\nPlecarea: ");
				strcat(raspuns, trainsInfo[i].dep_time);
				strcat(raspuns, "\nSosirea: ");
				strcat(raspuns, trainsInfo[i].arr_time);
				for(int q = 0; q < trainsInfo[i].no_of_stations; ++q){
					strcat(raspuns, "\n\n*Statia ");
					char station_ord[4];
					sprintf(station_ord, "%d", q+1);
					strcat(raspuns, station_ord);
					strcat(raspuns, ": ");
					strcat(raspuns, trainsInfo[i].ruta[q].stat_name);
					strcat(raspuns, "\nAjunge la ");
					strcat(raspuns, trainsInfo[i].ruta[q].arr_time);
					strcat(raspuns, "\nIntarziere: ");
					if(strcmp(trainsInfo[i].ruta[q].delay, "0") == 0) strcat(raspuns, "NU");
					else strcat(raspuns, trainsInfo[i].ruta[q].delay);
				}
				strcat(raspuns, "\n");
			}
			pthread_mutex_unlock(&mutex[0]);
		}
		else if(strstr(comanda, "plecari_in_ora") != NULL || strcmp(comanda, "3\n") == 0){
			// daca dupa comanda este un nume de oras scriem plecarile din acel oras, altfel scriem toate plecarile intr-o ora
			char h[3], m[3];
			bool oras = false;
			for(int i = 0; i < 2; ++i){
				h[i] = timp_actual[i];
                m[i] = timp_actual[i+3];
			}
			h[2] = '\0';
			m[2] = '\0';
			int ore = atoi(h), minute = atoi(m);
			strcat(raspuns, "[RASPUNS] Plecări în următoarea oră ");
			char cityName[50];
			if(strcmp(comanda, "plecari_in_ora\n") != 0 && strcmp(comanda, "3\n") != 0){
				strcat(raspuns, "din ");
				strcpy(cityName, &comanda[15]);
				cityName[strlen(cityName) - 1] = '\0';
				strcat(raspuns, cityName);
				strcat(raspuns, " ");
				oras = true;
			}
			strcat(raspuns, "(");
			char peste_o_ora[6];
			strcpy(peste_o_ora, timp_actual);
			peste_o_ora[5] = '\0';
			strcat(raspuns, peste_o_ora);
			strcat(raspuns, "-");
			ore++;
			if(ore >= 24) ore -= 24;
 			if(ore > 9 && minute > 9) sprintf(peste_o_ora, "%d:%d", ore, minute);
			else if(ore > 9 && minute <= 9) sprintf(peste_o_ora, "%d:0%d", ore, minute);
			else if(ore <= 9 && minute > 9) sprintf(peste_o_ora, "0%d:%d", ore, minute);
			else if(ore <= 9 && minute <= 9) sprintf(peste_o_ora, "0%d:0%d", ore, minute);
			strcat(raspuns, peste_o_ora);
			strcat(raspuns, "):\n");
			ore--;
			if(ore < 0) ore += 24;
			if(oras){
				pthread_mutex_lock(&mutex[1]);
				for(int i = 0; i < no_trains; ++i){
					bool exista_statie = false;
					for(int q = 0; q < trainsInfo[i].no_of_stations; ++q){
						if(strcmp(trainsInfo[i].ruta[q].stat_name, cityName) == 0) exista_statie = true;
					}
					printf("&%s&", cityName);
					if(exista_statie){
						bool firstTime = true;
						for(int q = 0; q < trainsInfo[i].no_of_stations; ++q){
							if(strcmp(trainsInfo[i].ruta[q].stat_name, cityName) == 0){
								char temp_time_container[5];
								char arrival_time[6], ha[3], ma[3];
								strcpy(arrival_time, trainsInfo[i].ruta[q].arr_time);
								arrival_time[5] = '\0';
								for(int x = 0; x < 2; ++x){
									ha[x] = arrival_time[x];
									ma[x] = arrival_time[x+3];
								}
								ha[2] = '\0';
								ma[2] = '\0';
								int arr_ore = atoi(ha), arr_minute = atoi(ma) + TRENUL_ASTEAPTA;
								if(arr_minute >= 60){
									arr_ore += arr_minute / 60;
									arr_minute = arr_minute % 60;
								}
								if(arr_ore >= 24) arr_minute -= 24;
								if((((arr_ore % 24) == (ore % 24) + 1) && (arr_minute <= minute)) || (((arr_ore % 24) == (ore % 24)) && (arr_minute >= minute))){
									if(firstTime){
										strcat(raspuns, "*Trenul ");
										strcat(raspuns, trainsInfo[i].id);
										strcat(raspuns, ":\n");
										firstTime = false;
									}
									strcat(raspuns, "  ");
									strcat(raspuns, trainsInfo[i].ruta[q].stat_name);
									strcat(raspuns, " -> ");
									if(q == trainsInfo[i].no_of_stations - 1) strcat(raspuns, "CURSĂ SPECIALĂ(SPRE HANGAR) | ");
									else{
										strcat(raspuns, trainsInfo[i].ruta[q+1].stat_name);
										strcat(raspuns, " | ");
									}
									if(arr_ore > 9 && arr_minute > 9) sprintf(arrival_time, "%d:%d", arr_ore, arr_minute);
									else if(arr_ore > 9 && arr_minute <= 9) sprintf(arrival_time, "%d:0%d", arr_ore, arr_minute);
									else if(arr_ore <= 9 && arr_minute > 9) sprintf(arrival_time, "0%d:%d", arr_ore, arr_minute);
									else if(arr_ore <= 9 && arr_minute <= 9) sprintf(arrival_time, "0%d:0%d", arr_ore, arr_minute);
									strcat(raspuns, arrival_time);
									strcat(raspuns, "\n");
								}
							}
						}
					}
				}
				pthread_mutex_unlock(&mutex[1]);
			}
			else {
				pthread_mutex_lock(&mutex[1]);
				for(int y = 0; y < no_trains; ++y) {
					bool firstTime = true;
					for(int o = 0; o < trainsInfo[y].no_of_stations; ++o){
						char temp_time_container[5];
						char arrival_time[6], ha[3], ma[3];
						strcpy(arrival_time, trainsInfo[y].ruta[o].arr_time);
						arrival_time[5] = '\0';
						for(int x = 0; x < 2; ++x){
							ha[x] = arrival_time[x];
							ma[x] = arrival_time[x+3];
						}
						ha[2] = '\0';
						ma[2] = '\0';
						int arr_ore = atoi(ha), arr_minute = atoi(ma) + TRENUL_ASTEAPTA;
						if(arr_minute >= 60){
							arr_ore += arr_minute / 60;
							arr_minute = arr_minute % 60;
						}
						if(arr_ore >= 24) arr_minute -= 24;
						if((((arr_ore % 24) == (ore % 24) + 1) && (arr_minute <= minute)) || (((arr_ore % 24) == (ore % 24)) && (arr_minute >= minute))){
							if(firstTime){
								strcat(raspuns, "*Trenul ");
								strcat(raspuns, trainsInfo[y].id);
								strcat(raspuns, ":\n");
								firstTime = false;
							}
							strcat(raspuns, "  ");
							strcat(raspuns, trainsInfo[y].ruta[o].stat_name);
							strcat(raspuns, " -> ");
							if(o == trainsInfo[y].no_of_stations - 1) strcat(raspuns, "CURSĂ SPECIALĂ(SPRE HANGAR) | ");
							else{
								strcat(raspuns, trainsInfo[y].ruta[o+1].stat_name);
								strcat(raspuns, " | ");
							}
							if(arr_ore > 9 && arr_minute > 9) sprintf(arrival_time, "%d:%d", arr_ore, arr_minute);
							else if(arr_ore > 9 && arr_minute <= 9) sprintf(arrival_time, "%d:0%d", arr_ore, arr_minute);
							else if(arr_ore <= 9 && arr_minute > 9) sprintf(arrival_time, "0%d:%d", arr_ore, arr_minute);
							else if(arr_ore <= 9 && arr_minute <= 9) sprintf(arrival_time, "0%d:0%d", arr_ore, arr_minute);
							strcat(raspuns, arrival_time);
							strcat(raspuns, "\n");
						}
					}
				}
				pthread_mutex_unlock(&mutex[1]);
			}
		} 
		else if(strstr(comanda, "sosiri_in_ora") != NULL || strcmp(comanda, "4\n") == 0){
			bool oras = false;
			char h[3], m[3];
			for(int i = 0; i < 2; ++i){
				h[i] = timp_actual[i];
                m[i] = timp_actual[i+3];
			}
			h[2] = '\0';
			m[2] = '\0';
			int ore = atoi(h), minute = atoi(m);
			char cityName[50];
			strcat(raspuns, "[RASPUNS] Sosiri în următoarea oră ");
			if(strcmp(comanda, "sosiri_in_ora\n") != 0 && strcmp(comanda, "4\n") != 0){
				strcat(raspuns, "în ");
				strcpy(cityName, &comanda[14]);
				cityName[strlen(cityName) - 1] = '\0';
				strcat(raspuns, cityName);
				strcat(raspuns, " ");
				oras = true;
			}
			strcat(raspuns, "(");
			char peste_o_ora[6], timp_actual_cpy[6];
			strcpy(peste_o_ora, timp_actual);
			strcpy(timp_actual_cpy, timp_actual);
			peste_o_ora[5] = '\0';
			timp_actual_cpy[5] = '\0';
			strcat(raspuns, peste_o_ora);
			strcat(raspuns, "-");
			ore++;
			if(ore >= 24) ore -= 24;
			if(ore > 9 && minute > 9) sprintf(peste_o_ora, "%d:%d", ore, minute);
			else if(ore > 9 && minute <= 9)sprintf(peste_o_ora, "%d:0%d", ore, minute);
			else if(ore <= 9 && minute > 9)sprintf(peste_o_ora, "0%d:%d", ore, minute);
			else if(ore <= 9 && minute <= 9)sprintf(peste_o_ora, "0%d:0%d", ore, minute);
			strcat(raspuns, peste_o_ora);
			strcat(raspuns, "):\n");
			ore--;
			if(ore < 0) ore += 24;
			if(oras){
				pthread_mutex_lock(&mutex[2]);
				for(int i = 0; i < no_trains; ++i){
					bool exista_statie = false;
					for(int q = 0; q < trainsInfo[i].no_of_stations; ++q){
						if(strcmp(trainsInfo[i].ruta[q].stat_name, cityName) == 0){
							exista_statie = true;
							break;
						}
					}
					if(exista_statie){
						bool firstTime = true;
						for(int q = 0; q < trainsInfo[i].no_of_stations; ++q){
							if(strcmp(trainsInfo[i].ruta[q].stat_name, cityName) == 0){
								char temp_time_container[5];
								char arrival_time[6], ha[3], ma[3];
								strcpy(arrival_time, trainsInfo[i].ruta[q].arr_time);
								arrival_time[5] = '\0';
								for(int x = 0; x < 2; ++x){
									ha[x] = arrival_time[x];
									ma[x] = arrival_time[x+3];
								}
								ha[2] = '\0';
								ma[2] = '\0';
								int arr_ore = atoi(ha), arr_minute = atoi(ma);
								if(arr_minute >= 60){
									arr_ore += arr_minute / 60;
									arr_minute = arr_minute % 60;
								}
								if(arr_ore >= 24) arr_minute -= 24;
								if((((arr_ore % 24) == (ore % 24) + 1) && (arr_minute <= minute)) || (((arr_ore % 24) == (ore % 24)) && (arr_minute >= minute))){
									if(firstTime){
										strcat(raspuns, "*Trenul ");
										strcat(raspuns, trainsInfo[i].id);
										strcat(raspuns, ":\n");
										firstTime = false;
									}
									strcat(raspuns, "  ");
									if(q > 0) strcat(raspuns, trainsInfo[i].ruta[q-1].stat_name);
									else strcat(raspuns, "CURSĂ SPECIALĂ(DIN HANGAR)");
									strcat(raspuns, " -> ");
									strcat(raspuns, trainsInfo[i].ruta[q].stat_name);
									strcat(raspuns, " | ");
									if(arr_ore > 9 && arr_minute > 9) sprintf(arrival_time, "%d:%d", arr_ore, arr_minute);
									else if(arr_ore > 9 && arr_minute <= 9) sprintf(arrival_time, "%d:0%d", arr_ore, arr_minute);
									else if(arr_ore <= 9 && arr_minute > 9) sprintf(arrival_time, "0%d:%d", arr_ore, arr_minute);
									else if(arr_ore <= 9 && arr_minute <= 9) sprintf(arrival_time, "0%d:0%d", arr_ore, arr_minute);
									strcat(raspuns, arrival_time);
									strcat(raspuns, "\n");
								}
							}
						}
					}
				}
				pthread_mutex_unlock(&mutex[2]);
			}
			else {
				pthread_mutex_lock(&mutex[2]);
				for(int y = 0; y < no_trains; ++y) {
					bool firstTime = true;
					for(int o = 0; o < trainsInfo[y].no_of_stations; ++o){
						char temp_time_container[5];
						char arrival_time[6], ha[3], ma[3];
						strcpy(arrival_time, trainsInfo[y].ruta[o].arr_time);
						arrival_time[5] = '\0';
						for(int x = 0; x < 2; ++x){
							ha[x] = arrival_time[x];
							ma[x] = arrival_time[x+3];
						}
						ha[2] = '\0';
						ma[2] = '\0';
						int arr_ore = atoi(ha), arr_minute = atoi(ma);
						if(arr_minute >= 60){
							arr_ore += arr_minute / 60;
							arr_minute = arr_minute % 60;
						}
						if(arr_ore >= 24) arr_minute -= 24;
						if((((arr_ore % 24) == (ore % 24) + 1) && (arr_minute <= minute)) || (((arr_ore % 24) == (ore % 24)) && (arr_minute >= minute))){
							if(firstTime){
								strcat(raspuns, "*Trenul ");
								strcat(raspuns, trainsInfo[y].id);
								strcat(raspuns, ":\n");
								firstTime = false;
							}
							strcat(raspuns, "  ");
							if(o == 0) strcat(raspuns, "CURSĂ SPECIALĂ(SPRE HANGAR)");
							else strcat(raspuns, trainsInfo[y].ruta[o - 1].stat_name);
							strcat(raspuns, " -> ");
							strcat(raspuns, trainsInfo[y].ruta[o].stat_name);
							strcat(raspuns, " | ");
							if(arr_ore > 9 && arr_minute > 9) sprintf(arrival_time, "%d:%d", arr_ore, arr_minute);
							else if(arr_ore > 9 && arr_minute <= 9) sprintf(arrival_time, "%d:0%d", arr_ore, arr_minute);
							else if(arr_ore <= 9 && arr_minute > 9) sprintf(arrival_time, "0%d:%d", arr_ore, arr_minute);
							else if(arr_ore <= 9 && arr_minute <= 9) sprintf(arrival_time, "0%d:0%d", arr_ore, arr_minute);
							strcat(raspuns, arrival_time);
							strcat(raspuns, "\n");
						}
					}
				}
				pthread_mutex_unlock(&mutex[2]);
			}
		} 
		else if(strstr(comanda, "intarzieri") != NULL || strcmp(comanda, "5\n") == 0){
			strcat(raspuns, "[RASPUNS] Întârzieri");
			char cityName[50];
			bool oras = false;
			if(strcmp(comanda, "intarzieri\n") != 0 && strcmp(comanda, "5\n") != 0){
				strcpy(cityName, &comanda[11]);
				strcat(raspuns, " la stația ");
				cityName[strlen(cityName) - 1] = '\0';
				strcat(raspuns, cityName);
				oras = true;
			}
			strcat(raspuns, ":\n");
			if(!oras){
				pthread_mutex_lock(&mutex[3]);
				for(int i = 0; i < no_trains; ++i){
					strcat(raspuns, "*Trenul ");
					strcat(raspuns, trainsInfo[i].id);
					strcat(raspuns, ": ");
					bool delay_found = false;
					for(int z = 0; z < trainsInfo[i].no_of_stations; ++z){
						if(strcmp(trainsInfo[i].ruta[z].delay, "0") != 0){
							delay_found = true;
							if(z > 0) strcat(raspuns, "            ");
							strcat(raspuns, "va întârzia la stația \"");
							strcat(raspuns, trainsInfo[i].ruta[z].stat_name);
							strcat(raspuns, "\" cu ");
							strcat(raspuns, trainsInfo[i].ruta[z].delay);
							strcat(raspuns, " minute | ");
							strcat(raspuns, trainsInfo[i].ruta[z].arr_time);
							strcat(raspuns, "\n");
						}
					}
					if(!delay_found) strcat(raspuns, "fără întârzieri\n");
				}
				pthread_mutex_unlock(&mutex[3]);
			}
			else{
				pthread_mutex_lock(&mutex[3]);
				for(int i = 0; i < no_trains; ++i){
					bool firstTime = true;
					for(int z = 0; z < trainsInfo[i].no_of_stations; ++z){
						if(strcmp(trainsInfo[i].ruta[z].stat_name, cityName) == 0){
							if(strcmp(trainsInfo[i].ruta[z].delay, "0") != 0){
								if(firstTime){
									strcat(raspuns, "*Trenul ");
									strcat(raspuns, trainsInfo[i].id);
									strcat(raspuns, ": ");
									firstTime = false;
								}
								if(z > 0) strcat(raspuns, "            ");
								strcat(raspuns, "va întârzia la stația \"");
								strcat(raspuns, trainsInfo[i].ruta[z].stat_name);
								strcat(raspuns, "\" cu ");
								strcat(raspuns, trainsInfo[i].ruta[z].delay);
								strcat(raspuns, " minute | ");
								strcat(raspuns, trainsInfo[i].ruta[z].arr_time);
								strcat(raspuns, "\n");
							}
						}
					}
				}
				pthread_mutex_unlock(&mutex[3]);
			}
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
				//statie_client
				bool train_found = false, station_found = false; 
				for(int k = 0; k < 4; ++k) pthread_mutex_lock(&mutex[k]);
				for(int i = 0; i < no_trains; ++i){
					if(strcmp(trainsInfo[i].id, train_id) == 0){
						train_found = true;
						char ariv_ore[3], ariv_minute[3];
						for(int q = 0; q < trainsInfo[i].no_of_stations; ++q){
							if(strcmp(trainsInfo[i].ruta[q].stat_name, statie_client) == 0){
								station_found = true;
								for(int j = 0; j < 2; ++j){
									ariv_ore[j] = trainsInfo[i].ruta[q].arr_time[j];
									ariv_minute[j] = trainsInfo[i].ruta[q].arr_time[j+3];
								}
								ariv_ore[2] = '\0';
								ariv_minute[2] = '\0';
								char a_ore[3], a_minute[3];
								for(int j = 0; j < 2; ++j){
									a_ore[j] = timp_actual[j];
									a_minute[j] = timp_actual[j+3];
								}
								a_ore[2] = '\0';
								a_minute[2] = '\0';
								int ora_actual = atoi(a_ore);
								int minute_actual = atoi(a_minute);
								if(ora_actual > atoi(ariv_ore) || (ora_actual >= atoi(ariv_ore) && minute_actual >=  atoi(ariv_minute))){
									sentAnuntIntarziere(csd, train_id, tdelay, statie_client);
									char total_delay[4];
									sprintf(total_delay, "%d", atoi(trainsInfo[i].ruta[q].delay) + atoi(tdelay));
									strcpy(trainsInfo[i].ruta[q].delay, total_delay);
									for(int x = q; x < trainsInfo[i].no_of_stations; ++x){
										char mins[3], hours[3];
										for(int g = 0; g < 2; ++g){
											mins[g] = trainsInfo[i].ruta[x].arr_time[g + 3];
											hours[g] = trainsInfo[i].ruta[x].arr_time[g];
										}
										mins[2] = '\0';
										hours[2] = '\0';
										int new_m = atoi(mins) + atoi(tdelay);
										int new_h = atoi(hours);
										printf("Before change: %d:%d\n", new_h, new_m);
										if(new_m >= 60){
											new_h += new_m / 60; 
											new_m = new_m % 60;
											if(new_h >= 24) new_h -= 24;
										}
										printf("After change: %d:%d\n", new_h, new_m);
										char new_arr[6];
										if(new_h > 9) sprintf(hours, "%d", new_h);
										else sprintf(hours, "0%d", new_h);
										if(new_m > 9) sprintf(mins, "%d", new_m);
										else sprintf(mins, "0%d", new_m);
										mins[2] = '\0';
										hours[2] = '\0';
										sprintf(new_arr, "%s:%s", hours, mins);
										new_arr[5] = '\0';
										//printf("^newarr = %s^\n", new_arr);
										strcpy(trainsInfo[i].ruta[x].arr_time, new_arr);
										strcpy(trainsInfo[i].ruta[x].delay, total_delay);
									}
									if(strcmp(trainsInfo[i].ruta[0].stat_name, statie_client) == 0){
										char h[3], m[3], new_dep[6];
										int new_m = atoi(ariv_minute) + atoi(tdelay);
										int new_h = atoi(ariv_ore);
										if(new_m >= 60){
											new_h += new_m / 60; 
											new_m = new_m % 60;
											if(new_h >= 24) new_h -= 24;
										}
										if(new_h > 9) sprintf(h, "%d", new_h);
										else sprintf(h, "0%d", new_h);
										if(new_m > 9) sprintf(m, "%d", new_m);
										else sprintf(m, "0%d", new_m);
										h[2] = '\0';
										m[2] = '\0';
										sprintf(new_dep, "%s:%s", h, m);
										new_dep[5] = '\0';
										strcpy(trainsInfo[i].dep_time, new_dep);
									}
									char h[3], m[3], new_arr[6];
									for(int u = 0; u < 2; ++u){
										h[u] = trainsInfo[u].dep_time[u + 3];
										m[u] = trainsInfo[u].dep_time[u];
									}
									int new_m = atoi(m) + atoi(tdelay);
									int new_h = atoi(h);
									if(new_m >= 60){
										new_h += new_m / 60; 
										new_m = new_m % 60;
										if(new_h >= 24) new_h -= 24;
									}
									if(new_h > 9) sprintf(h, "%d", new_h);
									else sprintf(h, "0%d", new_h);
									if(new_m > 9) sprintf(m, "%d", new_m);
									else sprintf(m, "0%d", new_m);
									h[2] = '\0';
									m[2] = '\0';
									sprintf(new_arr, "%s:%s", h, m);
									new_arr[5] = '\0';
									strcpy(trainsInfo[i].arr_time, new_arr);
									strcat(raspuns, "[RASPUNS] Raportare înregistrată. Trenul ");
									strcat(raspuns, train_id);
									strcat(raspuns, " a întârziat cu ");
									strcat(raspuns, tdelay);
									strcat(raspuns, " minute.\n");
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
							else if(!station_found && q == trainsInfo[i].no_of_stations - 1){
								strcat(raspuns, "[EROARE] Statia ");
								strcat(raspuns, statie_client);
								strcat(raspuns, " nu a fost găsită în ruta trenului ");
								strcat(raspuns, train_id);
								strcat(raspuns, "\n");
							}
						}
					}
					else if (!train_found && i == no_trains - 1){
						strcat(raspuns, "[EROARE] Trenul ");
						strcat(raspuns, train_id);
						strcat(raspuns, " nu a fost găsit.\n");
					}
				}
				for(int k = 0; k < 4; ++k) pthread_mutex_unlock(&mutex[k]);
			}
		}
		else strcat(raspuns, "[EROARE] Comandă inexistentă.\n");

		if(write(csd, raspuns, s) == -1){
			perror("Eroare write to client");
			break;
		}
	}
	//printf("\n{{{{}}}}}%s{{{{}}}}\n", trainsInfo[0].id);
	if(close(csd) == -1) perror("Eroare la close.");
	pthread_exit(NULL);
}

void* time_simulation(void* arg){
	float minute = 1.0/60.0;
	while(1){
		server_timef += minute;
		usleep(500000); 
		//printf("%s\n", timp_actual);
		if(server_timef >= 24.0){
			server_timef = 0.0;
			for(int k = 0; k < 4; ++k) pthread_mutex_lock(&mutex[k]);
			reset_schedule();
			for(int k = 0; k < 4; ++k) pthread_mutex_unlock(&mutex[k]);
		}
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

int GetNoOfStations(xmlNodePtr tren){
	tren = tren->next;
	//xmlNodePtr trenInfo = tren->children;
	int count = 0;
	while(tren != NULL) {
		//printf("{%s}", tren->name);
		if(xmlStrcmp(tren->name, (const xmlChar*)"statie") == 0) count++;
		tren = tren->next;
	}
	//printf("|{%d}\n", count);
	return count;
}

struct Tren* getInfoSchedule(int no_of_trains){
	xmlDocPtr schedule;
	xmlNodePtr root;
	schedule = xmlReadFile("schedule.xml", NULL, 0);
	if(schedule == NULL){
		perror("Eroare la citirea fisierului schedule.xml\n");
	}
	root = xmlDocGetRootElement(schedule);
	if(root == NULL){
		perror("Fisierul xml e gol\n");
    }
	struct Tren* TrenuriInfo = malloc(sizeof(struct Tren) * no_of_trains);
	int j = 0;
	xmlNodePtr trenuri = root->children;
	while(trenuri != NULL){
		//printf("A");
		if(trenuri->type == XML_ELEMENT_NODE && xmlStrcmp(trenuri->name, (const xmlChar*)"train") == 0){
			//printf("TREN\n");
			xmlNodePtr train = trenuri->children;
			int no_of_stations = GetNoOfStations(train);
			//printf("T%dT\n", no_of_stations);
			struct Tren tr;
			struct Statie* stations = malloc(sizeof(struct Statie) * no_of_stations); 
			tr.no_of_stations = no_of_stations;
			int i = 0;
			while(train != NULL){
				//printf("^%s^\n", train->name);
				if(xmlStrcmp(train->name, (const xmlChar*)"id") == 0){
					strcpy(tr.id, xmlNodeGetContent(train));
					tr.id[strlen(tr.id)] = '\0';
					//printf("tr.id: %s\n", tr.id);
				}
				else if(xmlStrcmp(train->name, (const xmlChar*)"from") == 0){
					strcpy(tr.from, xmlNodeGetContent(train));
					tr.from[strlen(tr.from)] = '\0';
					//printf("tr.from: %s\n", tr.from);
				}
				else if(xmlStrcmp(train->name, (const xmlChar*)"to") == 0){
					strcpy(tr.to, xmlNodeGetContent(train));
					tr.to[strlen(tr.to)] = '\0';
					//printf("tr.to: %s\n", tr.to);
				}
				else if(xmlStrcmp(train->name, (const xmlChar*)"departure") == 0){
					strcpy(tr.dep_time, xmlNodeGetContent(train));
					tr.dep_time[strlen(tr.dep_time)] = '\0';
					//printf("tr.dep_time: %s\n", tr.dep_time);
				}
				else if(xmlStrcmp(train->name, (const xmlChar*)"arrival") == 0){
					strcpy(tr.arr_time, xmlNodeGetContent(train));
					tr.arr_time[strlen(tr.arr_time)] = '\0';
					//printf("tr.arr_time: %s\n", tr.arr_time);
				}
				else if(xmlStrcmp(train->name, (const xmlChar*)"statie") == 0){
					xmlNodePtr statie = train->children;
					struct Statie* statieInfo = malloc(sizeof(struct Statie));
					while(statie != NULL){
						//printf("%d|",j);
						if(statie->type == XML_ELEMENT_NODE){
							if(xmlStrcmp(statie->name, (const xmlChar*)"name") == 0){
								strcpy(statieInfo->stat_name, xmlNodeGetContent(statie));
								statieInfo->stat_name[strlen(statieInfo->stat_name)] = '\0';
								//printf("statieInfo.stat_name: %s\n", statieInfo->stat_name);
							}
							else if(xmlStrcmp(statie->name, (const xmlChar*)"arrival_time") == 0){
								strcpy(statieInfo->arr_time, xmlNodeGetContent(statie));
								statieInfo->arr_time[strlen(statieInfo->arr_time)] = '\0';
								//printf("statieInfo.arr_time: %s\n", statieInfo->arr_time);
							}
							else if(xmlStrcmp(statie->name, (const xmlChar*)"arrival_delay") == 0){
								strcpy(statieInfo->delay, xmlNodeGetContent(statie));
								statieInfo->delay[strlen(statieInfo->delay)] = '\0';
								//printf("statieInfo.delay: %s\n", statieInfo->delay);
							}
						}
						statie = statie->next;
					}
					stations[i] = *statieInfo;
					++i;
				}
				train = train->next;
			}
			tr.ruta = stations;
			TrenuriInfo[j] = tr;
			++j;
		}
		//if(strcmp(xmlNodeGetContent(temp), train_id) == 0){}
		trenuri = trenuri->next;
	}
	return TrenuriInfo;
}

void reset_schedule(){
	xmlDocPtr schedule;
	xmlNodePtr root;
	schedule = xmlReadFile("schedule.xml", NULL, 0);
	if(schedule == NULL){
		perror("Eroare la citirea fisierului schedule.xml\n");
	}
	root = xmlDocGetRootElement(schedule);
	if(root == NULL){
		perror("Fisierul xml e gol\n");
    }
	int no_trains = getNoOfTrains(root);
	free(trainsInfo);
	trainsInfo = malloc(sizeof(struct Tren) * no_trains);
	trainsInfo = getInfoSchedule(no_trains);
}


int main(){
	pthread_mutex_init(&mutex[0], NULL);
	pthread_mutex_init(&mutex[1], NULL);
	pthread_mutex_init(&mutex[2], NULL);
	pthread_mutex_init(&mutex[3], NULL);
	xmlDocPtr schedule;
	xmlNodePtr root;
	schedule = xmlReadFile("schedule.xml", NULL, 0);
	if(schedule == NULL){
		perror("Eroare la citirea fisierului schedule.xml\n");
	}
	root = xmlDocGetRootElement(schedule);
	if(root == NULL){
		perror("Fisierul xml e gol\n");
    }
	no_trains = getNoOfTrains(root);
	//printf("%d\n", no_trains);
	trainsInfo = malloc(sizeof(struct Tren) * no_trains);
	trainsInfo = getInfoSchedule(no_trains);
	//printf("$|%s| -> |%s|$\n", trainsInfo[1].ruta[0].arr_time, trainsInfo[2].id);

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

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
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

    printf("ip: %d \t port: %d\n", server.sin_addr.s_addr, PORT);
    fflush(stdout);

    if (listen(sd, MAX_CL) == -1){
    	perror("Eroare la listen().\n");
    	return errno;
    }

    while(1){
    	int client;
    	int length = sizeof(from);
		while(threads_count > MAX_CL){
			pthread_join(threads[threads_count - 1], NULL);
			--threads_count;
		}
    	//printf ("Asteptam la portul %d...\n",PORT);
    	//fflush (stdout);
    	/* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    	client = accept(sd, (struct sockaddr *) &from, &length);
		clienti[threads_count] = client;
		//printf("%d = %d", threads_count, clienti[threads_count]);
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
    }
	//threads_count = 0;
	while(threads_count < MAX_CL){
		pthread_join(threads[threads_count], NULL);
		//threads_count++;
	}
	pthread_mutex_destroy(&mutex[0]);
	pthread_mutex_destroy(&mutex[1]);
	pthread_mutex_destroy(&mutex[2]);
	pthread_mutex_destroy(&mutex[3]);
	close(sd);
}