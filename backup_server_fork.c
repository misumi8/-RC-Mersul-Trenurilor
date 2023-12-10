        int pid;
    	if((pid = fork()) == -1){
    		close(client);
    		continue;
    	} 
		else if(pid > 0){
    		// parinte
    		close(client);
    		while(waitpid(-1,NULL,WNOHANG));
    		continue;
    	} 
		else if(pid == 0){
    		// copil
			close(sd);
			while(1){
				bzero(comanda, 100);
				printf("\nAsteptam mesajul\n");
				fflush(stdout);

				if (read(client, comanda, 100) <= 0){
					perror("Eroare la read() de la client sau clientul s-a deconectat.\n");
					close(client);	/* inchidem conexiunea cu clientul */
					break;
				}

				printf ("Mesajul primit: %s\n", comanda);

				bzero(raspuns, schedule_size);
				//implementeaeza comenzi
				strcat(raspuns, "un raspuns");
				/*raspuns = ...*/

				printf("Trimitem mesajul: %s\n", raspuns);

				if (write (client, raspuns, schedule_size) <= 0){
					perror("Eroare la write() catre client sau clientul s-a deconectat.\n");
					close(client);
					break;		
				}
				else printf("Mesajul a fost trasmis cu succes.\n");
			}
    		close (client);
    		exit(0);
    	}