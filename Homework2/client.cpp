#include <cstdio>
#include <iterator>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 

using namespace std;

// Define-uri folosite la debug, respectiv logging
#define BUFLEN 1024
#define DEBUG(X) cerr << #X" = " << X << '\n';
#define LOG(expr, flag)\
    if (flag == 1)\
        log << "> " << expr << '\n';\
    else\
        log << expr;
#define TRIM(s) s[strlen(s) - 1] = '\0';

// Sirurile de caractere folosite pentru erori
const string err1("-1 : Client inexistent\n");
const string err2("-2 : Fisier inexistent\n");
const string err3("-3 : Exista fisier local cu acelasi nume\n");
const string err4("-4 : Eroare la conectare\n");
const string err5("-5 : Eroare la citire de pe socket\n");

// Extrage primele n caractere din sirul str
char* substr(char *str, int n) {
    char* ret = new char[n + 1];
    strncpy(ret, str, n + 1);
    ret[n] = '\0';
    return ret;
}

void error(char *msg)
{
    perror(msg);
    exit(0);
}

// Returneaza timpul curent formatat conform conditiilor din enunt
char *formatedTime() {
    const time_t now = time(0); 
    struct tm* info = localtime(&now);
    char *formated = (char*)malloc(80 * sizeof(char));
    strftime(formated, 80, "%I:%M:%S %p", info);
    return formated;
}

int main(int argc, char *argv[])
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[BUFLEN];

    if (argc < 3) {
        fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
        exit(0);
    }

    /**
     * Deschid un socket pe care sa asculte clientul,
     * in mod asemanator cu felul in care e deschis pe server
     */
    int client_server_sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    assert(client_server_sockfd >= 0);
    struct sockaddr_in client_server_addr;
    memset((char*)&client_server_addr, 0, sizeof(client_server_addr));
    client_server_addr.sin_family = AF_INET;
    client_server_addr.sin_addr.s_addr = INADDR_ANY;
    client_server_addr.sin_port = htons(atoi(argv[3]));
    assert(bind(client_server_sockfd, (struct sockaddr*) &client_server_addr, sizeof(struct sockaddr)) >= 0);
    assert(listen(client_server_sockfd, 200) >= 0);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error((char*)"ERROR opening socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[5]));
    inet_aton(argv[4], &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) 
        error((char*)"ERROR connecting"); 

    int pos = 5;
    strcpy(buffer, "init");
    buffer[5] = '\0';
    for(int i = 1; i <= 3; i++) {
        int len = strlen(argv[i]) + 1;
        memcpy(buffer + pos, &len, sizeof(int));
        pos += sizeof(int);
        memcpy(buffer + pos, argv[i], len);
        pos += len;
    } 

    // Trimit mesajul initial catre server
    char* time_stamp = formatedTime();
    int len = strlen(time_stamp);
    memcpy(buffer + pos, &len, sizeof(int));
    pos += sizeof(int);
    memcpy(buffer + pos, time_stamp, len + 1);
    pos += len + 1;
    assert(send(sockfd, buffer, pos, 0) >= 0);

    fd_set read_set;
    FD_SET(sockfd, &read_set);
    int cnt = select(sockfd + 1, &read_set, NULL, NULL, NULL);

    // Daca serverul imi da quit inseamna ca exista un client cu numele asta
    if(FD_ISSET(sockfd, &read_set)) {
        memset(buffer, 0, sizeof(buffer));
        recv(sockfd, buffer, BUFLEN, 0);
        if(strstr(buffer, "quit"))
            return 0;
    } 

    ofstream log(strcat(strdup(argv[1]), strdup(".log")));

    /* Used for sending files directly to other clients */
    int send_sockfd;
    struct sockaddr_in dest_addr;
    memset((char*)&dest_addr, 0, sizeof(client_server_addr));

    int sending_file = 0;
    string file_name;

    int fdmax = max(sockfd, client_server_sockfd);
    fd_set tmp_read_set;
    FILE *file_to_send = NULL, *file_to_receive = NULL;

    FD_SET(sockfd, &read_set);
    FD_SET(0, &read_set);
    FD_SET(client_server_sockfd, &read_set);

    while(1) {
        // citesc de la tastatura
        tmp_read_set = read_set;
        struct timeval wait_time;
        wait_time.tv_usec = 100000;

        // astept wait time secunde pana cand primesc o noua conexiune
        cnt = select(fdmax + 1, &tmp_read_set, NULL, NULL, &wait_time);
        if(cnt == 0 && sending_file) {
            // Sending_file == -1 inseamna ca fisierul nu exista 
            if(sending_file == -1) {
                int size = sending_file;
                memset(buffer, 0, sizeof(buffer)); 
                strcpy(buffer, "chunk");
                // Trimit un chunk cu size = -1 pentru a spune celuilalt client ca fisierul
                // cerut nu exista
                memcpy(buffer + 5, &size, sizeof(int));
                if (send(send_sockfd, buffer, 5 + sizeof(int), 0) < 0) {
                    LOG(err5, 0); 
                }
                sending_file = 0;
            } else {
                // Trimit un chunk de 256 bytes. 
                // Mesajul are forma chunk size info
                // Size este copiat cu memcpy exact dupa chunk
                // iar info dupa size
                memset(buffer, 0, sizeof(buffer));
                char aux[BUFLEN];
                int size = fread(aux, 1, 256, file_to_send);
                // cerr << "Sending " << size << " bytes\n";
                strcpy(buffer, "chunk");
                memcpy(buffer + 5, &size, sizeof(int));
                memcpy(buffer + 5 + sizeof(int), aux, size);
                // In cazul in care mesajul nu se poate trimite se opreste trimitrea
                // Cel mai probabil clientul catre care trimit s-a deconectat
                if(send(send_sockfd, buffer, 5 + sizeof(int) + size, 0) < 0) {
                    LOG(err5, 0);
                    sending_file = 0;
                    file_name = "";
                }
                // Daca size e diferit de 256 inseamna ca am trimis ultima bucata din fisier
                // si opresc trimiterea
                if(size != 256) {
                    sending_file = 0;
                    file_name = "";
                }
            }

        } else 
            for(int i = 0; i <= fdmax; i++) {
                if(FD_ISSET(i, &tmp_read_set)) {
                    if(i == 0) {
                        fgets(buffer, BUFLEN, stdin); 
                        TRIM(buffer);
                        if(!strcmp(substr(buffer, 7), "getfile")) {
                            LOG(buffer, 1);
                            string str(buffer + 8); 
                            string destination = str.substr(0, str.find(' '));
                            file_name = str.substr(str.find(' ') + 1);

                            // Trimit infoclient catre server pentru a afla ip / port
                            // pentru clientul de la care va trebui sa primesc
                            memset(buffer, 0, sizeof(buffer));
                            str = "infoclient " + destination; 
                            strcpy(buffer, str.c_str());
                            if(send(sockfd, buffer, strlen(buffer), 0) < 0) {
                                LOG(err5, 0);
                            }

                            fd_set set;
                            FD_SET(sockfd, &set);
                            select(sockfd + 1, &set, NULL, NULL, NULL);
                            memset(buffer, 0, sizeof(buffer));
                            if (recv(sockfd, buffer, BUFLEN, 0) < 0) {
                                LOG(err5, 0);
                            }

                            str.assign(buffer);
                            string port, ip;

                            istringstream iss(str);
                            vector<string> tokens;

                            copy(istream_iterator<string>(iss),
                                 istream_iterator<string>(),
                                 back_inserter<vector<string>>(tokens));

                            // Daca clientul nu exista scriu eroarea in log
                            if(tokens[0] != destination) {
                                LOG(err1, 0);
                            } else {
                                ip = tokens[1];
                                port = tokens[2];

                                int rejected = 0;
                                struct stat file_info;
                                // Daca clientul exista verific existenta fisierului cerut la mine
                                string path = string(argv[2]) + file_name;
                                if(stat(path.c_str(), &file_info) != -1) {
                                    memset(buffer, 0, sizeof(buffer));
                                    cerr << "Fisier deja existent. Suprascrieti ? (y / n)\n";
                                    LOG(err3, 0);
                                    memset(buffer, 0, sizeof(buffer));
                                    gets(buffer);
                                    // In cazul in care nu se doreste suprascrierea anulez comanda
                                    if(tolower(buffer[0]) == 'n') {
                                        rejected = 1;
                                        LOG("Comanda anulata\n", 0);
                                    } else { 
                                        LOG("Suprascris\n", 0);
                                    }
                                }
                                // Daca comanda nu a fost anulata construiesc un mesaj de request
                                if(!rejected) {
                                    // Creez socket-ul prin care trimit request-ul
                                    int port_nr = atoi(port.c_str());
                                    char *ip_str = strdup(ip.c_str());
                                    send_sockfd = socket(AF_INET, SOCK_STREAM, 0);
                                    assert(send_sockfd > 0);
                                    dest_addr.sin_family = AF_INET;
                                    dest_addr.sin_port = htons(port_nr);
                                    inet_aton(ip.c_str(), &dest_addr.sin_addr);
                                    if(connect(send_sockfd, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
                                        LOG(err4, 0);
                                    }
                                    // Trimit infoclient catre server cu numele meu pentru a vedea care este
                                    // ip-ul meu
                                    memset(buffer, 0, sizeof(buffer));
                                    str = "infoclient " + string(argv[1]);
                                    strcpy(buffer, str.c_str());
                                    if(send(sockfd, buffer, strlen(buffer), 0) < 0) {
                                        LOG(err5, 0);
                                    } 

                                    select(sockfd + 1, &set, NULL, NULL, NULL);
                                    memset(buffer, 0, sizeof(buffer));
                                    if(recv(sockfd, buffer, BUFLEN, 0) < 0) {
                                        LOG(err5, 0);
                                    }
                                    str.assign(buffer); 

                                    istringstream iss1(str);
                                    tokens.clear();

                                    copy(istream_iterator<string>(iss1),
                                         istream_iterator<string>(),
                                         back_inserter<vector<string>>(tokens));

                                    if(tokens[0] == string(argv[1])) {
                                        ip = tokens[1];
                                        port = tokens[2];
                                    }

                                    // Mesajul de request are forma begin ip port nume_fisier
                                    str = "begin " + ip + " " + port + " " + file_name;
                                    memset(buffer, 0, sizeof(buffer));
                                    strcpy(buffer, str.c_str());
                                    if(send(send_sockfd, buffer, strlen(buffer), 0) < 0) {
                                        LOG(err5, 0);
                                    }
                                }
                            }
                        }
                        else if(strstr(buffer, "sharefile")) {
                            int pos = strstr(buffer, "sharefile") - buffer;
                            struct stat file_info;
                            char *path = strcat(strdup(argv[2]), buffer + pos + 10); 
                            LOG(buffer, 1);
                            // Verific daca fisierul exista
                            if(stat(path, &file_info) != -1) {
                                const char* suffixes[] = {"B", "KB", "MB"};
                                int idx = 0;
                                long long size = file_info.st_size;
                                while(size >= 1024) {
                                    size /= 1024;
                                    i++;
                                } 
                                // Determin sufixul care trebuie atasat dimensiunii
                                sprintf(buffer + strlen(buffer), " %lld%s", size, suffixes[i]);
                                if(send(sockfd, buffer, strlen(buffer), 0) < 0) {
                                    LOG(err5, 0);
                                }
                                if(pos == 0) { 
                                    strcpy(buffer, "Succes\n");
                                    LOG(buffer, 0);
                                }
                            } else {
                                // In cazul in care fisierul nu exista scriu eroarea 
                                string tmp = err2;
                                strcpy(buffer, tmp.c_str());
                                LOG(buffer, 0);
                            }
                        } else {
                            // Orice comanda primesc o trimit la server
                            if(send(sockfd, buffer, strlen(buffer), 0) < 0) {
                                LOG(err5, 0);
                            }
                            // Daca primesc quit inchid fisierul de log si ies din program
                            LOG(buffer, 1);
                            if(!strcmp(buffer, "quit")) {
                                log.close();
                                return 0;
                            }
                        }
                    } else if(i == sockfd) {
                        // Daca am primit mesaj de la server il scriu in log
                        memset(buffer, 0, sizeof(buffer));
                        if(recv(sockfd, buffer, BUFLEN, 0) < 0) {
                            LOG(err5, 0);
                        }
                        // Daca e quit inchid si ies din program
                        if(strstr(buffer, "quit")) {
                            log.close();
                            return 0;
                        }
                        LOG(buffer, 0);
                    } else if(i == client_server_sockfd) {
                        // Daca am primit un mesaj pe socket-ul inactiv
                        // Inseamna ca se va conecta un nou client la mine
                        struct sockaddr_in cli_addr;
                        size_t clilen = sizeof(cli_addr);
                        int newsockfd = accept(client_server_sockfd, (struct sockaddr *)&cli_addr, (unsigned int*)&clilen);
                        assert(newsockfd != -1);
                        FD_SET(newsockfd, &read_set);
                        if (newsockfd > fdmax) 
                            fdmax = newsockfd;
                    } else { 
                        memset(buffer, 0, sizeof(buffer));
                        if(recv(i, buffer, BUFLEN, 0) < 0) {
                            LOG(err5, 0);
                        }
                        if(!strcmp(substr(buffer, 5), "begin")) {
                            // Procesez request-ul de la client
                            string str(buffer);
                            vector<string> tokens;
                            // Fac split la request
                            istringstream iss(str);
                            copy(istream_iterator<string>(iss),
                                 istream_iterator<string>(),
                                 back_inserter<vector<string>>(tokens));

                            // Creez socket-ul cu care voi trimtie catre clientul
                            // care a facut request
                            send_sockfd = socket(AF_INET, SOCK_STREAM, 0);
                            assert(send_sockfd > 0);
                            dest_addr.sin_family = AF_INET;
                            int port_nr = atoi(tokens[2].c_str());
                            string ip = tokens[1];
                            dest_addr.sin_port = htons(port_nr);
                            inet_aton(ip.c_str(), &dest_addr.sin_addr);
                            if(connect(send_sockfd, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
                                LOG(err4, 0);
                            }

                            // Daca deja trimit un fisier intorc cod de eroare -4
                            if(sending_file == 1) {
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "chunk");
                                int size = -4;
                                memcpy(buffer + 5, &size, sizeof(int));
                                if(send(send_sockfd, buffer, 5 + sizeof(int), 0) < 0)
                                    LOG(err5, 0);
                            } else {
                                string path = string(argv[2]) + tokens[3]; 
                                struct stat file_info;
                                // Daca fisierul exista trimit bucati din el
                                if(stat(path.c_str(), &file_info) != -1) {
                                    sending_file = 1;
                                    file_to_send = fopen(path.c_str(), "rb");
                                } else {
                                    // Daca nu exista voi trimite mesaj corespunzator
                                    sending_file = -1;
                                }
                            }
                        } else if(!strcmp(substr(buffer, 5), "chunk")) {
                            // Procesez chunk
                            int size = *(int*)(buffer + 5);
                            // Daca nu am deschis fisierul de scriere il deschid
                            if(file_to_receive == NULL) {
                                string path = string(argv[2]) + file_name;
                                file_to_receive = fopen(path.c_str(), "wb");
                            }
                            // Daca size este -1 inseamna ca fisierul nu exista
                            if(size == -1) {
                                LOG(err2, 0);
                            } else if(size == -4) {
                                LOG(err4, 0);
                            } else {
                                // Scriu numarul de octeti corespunzator chunk-ului primit
                                // cerr << "Written " << size << " bytes to file\n";
                                fwrite(buffer + 5 + sizeof(int), 1, size, file_to_receive);
                                fflush(file_to_receive);
                                // Daca size e diferit de 256 inseamna ca am primit ultima bucata de fisier
                                // si inchid conexiunea si fisierul de scriere
                                if(size != 256) {
                                    close(i);
                                    FD_CLR(i, &read_set);
                                    fclose(file_to_receive);
                                    file_to_receive = NULL;
                                    file_name = "";
                                    LOG("Succes\n", 0);
                                } 
                            }
                        }                     
                    }
                }
            }
    }

    close(client_server_sockfd);
    return 0;
}
