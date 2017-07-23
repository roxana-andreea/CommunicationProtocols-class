#include <cstdio>
#include <set>
#include <iostream>
#include <ctime>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client.h"
using namespace std;

#define MAX_CLIENTS 200
#define BUFLEN 1024
#define DEBUG(X) cout << #X" = " << X << '\n';
#define TRIM(str) str[strlen(str) - 1] = '\0'

const string err1("-1 : Client inexistent\n");
const string err2("-2 : Fisier inexistent\n");
const string err3("-3 : Exista fisier local cu acelasi nume\n");
const string err4("-4 : Eroare la conectare\n");
const string err5("-5 : Eroare la citire de pe socket\n");

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int extract(char *&dest, char *source) {
    int len = *(int*)source;
    dest = new char[len];
    memcpy(dest, source + sizeof(int), len);
    return len + sizeof(int);
}

char* substr(char *str, int n) {
    char* ret = new char[n + 1];
    strncpy(ret, str, n + 1);
    ret[n] = '\0';
    return ret;
}

int main(int argc, char *argv[])
{
     // Mapeaza fiecare obiect Client la socketul pe care scrie 
     map<int, Client> clients;
     // Mapeaza numele clientilor la socketuri
     map<string, int> client_names;
     // Mapeaza socket (Socketul pe care scrie un client) -> lista fisiere partajate de client
     map<int, map<string, string>> shared_files;

     int sockfd, newsockfd, portno, clilen;
     char buffer[BUFLEN];
     struct sockaddr_in serv_addr, cli_addr;
     int n, i, j;

     fd_set read_fds;	//multimea de citire folosita in select()
     fd_set tmp_fds;	//multime folosita temporar 
     int fdmax;		//valoare maxima file descriptor din multimea read_fds

     if (argc < 2) {
         fprintf(stderr,"Usage : %s port\n", argv[0]);
         exit(1);
     }

     //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
     FD_ZERO(&read_fds);
     FD_ZERO(&tmp_fds);
     
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error((char*)"ERROR opening socket");
     
     portno = atoi(argv[1]);
     memset((char *) &serv_addr, 0, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) 
              error((char*)"ERROR on binding");
     
     listen(sockfd, MAX_CLIENTS);

     //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
     FD_SET(sockfd, &read_fds);
     FD_SET(0, &read_fds);
     fdmax = sockfd;

     // main loop
	while (1) {
		tmp_fds = read_fds; 

		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
			error((char*)"ERROR in select");
	
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {	
                if (i == 0) {
                    // Citesc de la tastatura
                    fgets(buffer, BUFLEN, stdin);
                    TRIM(buffer);

                    // Daca e quit trimit quit la toti clientii
                    if(!strcmp(buffer, "quit")) {
                        for(j = sockfd + 1; j <= fdmax; j++) {
                            char aux[BUFLEN];
                            strcpy(aux, "quit");
                            // Verific daca indexul curent e un socket activ
                            if(FD_ISSET(j, &read_fds)) {
                                send(j, aux, strlen(aux), 0);
                            }
                        }
                        return 0;
                    }

                    if(!strcmp(buffer, "status")) {
                        for(auto it : clients)
                            cout << it.second << '\n';
                    }

                } else if (i == sockfd) {
                    // a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
                    // actiunea serverului: accept()
                    clilen = sizeof(cli_addr);
                    if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (unsigned int*)&clilen)) == -1) {
                        error((char*)"ERROR in accept");
                    } 
                    else {
                        //adaug noul socket intors de accept() la multimea descriptorilor de citire
                        FD_SET(newsockfd, &read_fds);
                        if (newsockfd > fdmax) { 
                            fdmax = newsockfd;
                        }
                    }

                    // Pentru a retine ip-ul cu care s-a conectat fiecare client
                    // instantiez un client fictiv in care este setata doar adresa ip
                    // si il mapez corespunzator socket-ului in care va scrie
                    // Restul informatiilor (nume, director, port) le asociez clientului
                    // printr-un mesaj primit de la acesta
                    clients.insert(make_pair(newsockfd, Client(string(), string(inet_ntoa(cli_addr.sin_addr)), string(), string(), string())));
                    printf("Noua conexiune de la %s, port %d, socket_client %d\n ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
                }

                else {
                    // am primit date pe unul din socketii cu care vorbesc cu clientii
                    //actiunea serverului: recv()
                    memset(buffer, 0, BUFLEN);
                    if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                        if (n == 0) {
                            //conexiunea s-a inchis
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            error((char*)"ERROR in recv");
                        }
                        close(i); 
                        string name = clients[i].get_name();
                        // Sterg intrarea socket - Client din map
                        clients.erase(i);
                        // Sterg intrarea socket - fisiere partajate
                        shared_files.erase(i);
                        // Sterg intrarea nume - socket
                        client_names.erase(name);                             
                        // Inchid socket-ul corespunzator clientului

                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
                    } 

                    else { //recv intoarce >0
                        printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);
                        if(!strcmp(buffer, "listclients")) {
                            string ret;
                            // Iau fiecare element din map-ul Clients (mapeaza socket -> Client)
                            for(auto client : clients)
                                // Creez un string cu numele clientilor urmat de \n
                                ret += client.second.get_name() + "\n";
                            // Copiez string -ul in buffer si trimit
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, ret.c_str());
                            send(i, buffer, strlen(buffer), 0);    
                        }

                        if(!strcmp(substr(buffer, 4), "init")) { 
                            // Acest mesaj este dat de fiecare client cand se conecteaza la server
                            // si contine informatiile legate de client
                            char *name, *time_stamp, *port, *dir;
                            // Extrag informatiile despre client
                            int idx = 5 + extract(name, buffer + 5); 
                            idx += extract(dir, buffer + idx);
                            idx += extract(port, buffer + idx);
                            // Time_stamp reprezinta momentul conectarii clientului la server
                            idx += extract(time_stamp, buffer + idx); 

                            // Daca exista deja un client cu acest nume se trimite quit catre clientul curent
                            // pentru ca acesta sa se inchida, deoarece nu se accepta duplicate de nume
                            if(client_names.find(name) != client_names.end()) {
                                clients.erase(i);
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "quit");
                                send(i, buffer, strlen(buffer), 0);
                            } else {
                                // Se creaza perechea nume, socket
                                client_names.insert(make_pair(string(name), i));
                                // Iau ip-ul clientului curent din clientul fictiv adaugat mai sus
                                string ip = clients[i].get_ip();
                                // Asociez socket-ului curent obiectul Client corespunzator
                                clients[i] = Client(name, ip, dir, port, time_stamp);
                                // Asociez numelui clientului curent socket-ul corespunzator
                                client_names[name] = i;
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "accepted");
                                send(i, buffer, strlen(buffer), 0);
                            }
                        }

                        if(!strcmp(substr(buffer, 10), "infoclient")) {
                            string ret;
                            // buffer + 10 reprezinta numele clientului
                            // iau socketul de pe care citeste clientul din map-ul client names
                            // si apoi iau obiectul client din map-ul clients
                            Client cl = clients[client_names[buffer + 11]];
                            // construiesc raspunsul pe baza obiectului client
                            ret += cl.get_name() + " " + cl.get_ip() + " " + cl.get_port() + " " + cl.get_time_stamp() + "\n"; 
                            // in cazul in care numele este vid inseamna ca nu exista respectivul client
                            if(cl.get_name() == "")
                                ret = err1; 
                            // copiez in buffer si trimit raspunsul
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, ret.c_str());
                            send(i, buffer, strlen(buffer), 0);
                        }

                        if(!strcmp(substr(buffer, 9), "sharefile")) {
                            // buffer + 10 reprezinta perechea nume_fisier, dimensiune
                            string str(buffer + 10);
                            int pos = str.find(' ');
                            // subsirul 0, pos reprzeinta numele fisierului
                            // restul reprezinta dimensiunea
                            // in map-ul shared_files modific cheia asociata socket-ului curent
                            // inserand o noua pereche <nume_fisier, dimensiune>
                            shared_files[i][str.substr(0, pos)] = str.substr(pos + 1);
                        }

                        if(!strcmp(substr(buffer, 11), "unsharefile")) {
                            // Similar cu share_file doar ca sterg perechea data de numele fisierului
                            // In cazul in care se da unshare pe un fisier care nu exista map-ul se ocupa
                            // singur de chestia asta din implementarea interna a functiei erase
                            string str(buffer + 12);
                            if(shared_files[i].find(str.substr(0, str.find(' '))) != shared_files[i].end()) {
                                shared_files[i].erase(str.substr(0, str.find(' ')));
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "Succes\n");
                            }
                            else {
                                memset(buffer, 0, sizeof(buffer)); 
                                strcpy(buffer, err2.c_str());  
                            }
                            send(i, buffer, strlen(buffer), 0);
                        }

                        if(!strcmp(substr(buffer, 8), "getshare")) {
                            // Daca exista clientul
                            if(client_names.find(buffer + 9) != client_names.end()) {
                                // Preiau map-ul cu fisiere (nume, dimensiune)
                                map<string, string> t = shared_files[client_names[buffer + 9]];
                                // Parcurg map-ul si creez string-ul de raspuns
                                string ret;
                                for(auto s : t)
                                    ret += s.first + " " + s.second + "\n";
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, ret.c_str());
                            } else {
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, err1.c_str());
                            }
                            send(i, buffer, strlen(buffer), 0);
                        }

                        if(!strcmp(substr(buffer, 8), "infofile")) {
                            string ret;
                            // Parcurg map-ul de fisiere
                            for(auto it : shared_files) {
                                // It e o pereche de tip socket, map<nume_Fisier, dimensiune>
                                // daca map-ul (al 2-lea membru din pereche) contine numele fisierului
                                if(it.second.find(buffer + 9) != it.second.end())
                                    // Atunci adaug informatiile despre Clientul mapat socket-ului i in sirul trimis ca raspuns
                                    ret += clients[it.first].get_name() + " " + string(buffer + 9) + " " + it.second[buffer + 9] + "\n";
                            }
                            memset(buffer, 0, sizeof(buffer));
                            // Daca sirul de raspuns e vid inseamna ca fisierul nu e partajat de nimeni si se intoarce
                            // eroarea corespunzatoare
                            if(ret == "")
                                strcpy(buffer, err2.c_str());
                            else
                                strcpy(buffer, ret.c_str());
                            send(i, buffer, strlen(buffer), 0);
                        }

                        if(!strcmp(buffer, "quit")) {
                            // Am primit quit de la un client
                            FD_CLR(i, &read_fds);
                            // Ii sterg socket-ul din descriptorii de citire
                            string name = clients[i].get_name();
                            // Sterg intrarea socket - Client din map
                            clients.erase(i);
                            // Sterg intrarea socket - fisiere partajate
                            shared_files.erase(i);
                            // Sterg intrarea nume - socket
                            client_names.erase(name);                             
                            // Inchid socket-ul corespunzator clientului
                            close(i);
                        }
                    }
                }
            }
        }
    }

    close(sockfd);

    return 0; 
}
