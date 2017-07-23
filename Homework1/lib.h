#ifndef LIB
#define LIB

#define NAME 1
#define LENGTH 2
#define ACC 3
#define INFO 4
#define PAYLOAD_SIZE 1400

typedef struct {
  // int type;
  int len;
  char payload[PAYLOAD_SIZE];
} msg;

typedef struct {
  char nr;
  char checksum;
  char payload[60];
} mesaj; 

char sum(char str[60], int len) {
  char ret = 0;
  int i = 0;
  for(i = 0; i < len; i++)
    ret ^= str[i];
  return ret;
}

void init(char* remote,int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message();
msg* receive_message_timeout(int timeout);

#endif

