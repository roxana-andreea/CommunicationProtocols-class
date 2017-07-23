#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include "lib.h"
#define HOST "127.0.0.1"
#define PORT 10000

int main(int argc,char** argv) {
	init(HOST,PORT);
	msg t, r;
	mesaj m;
 	FILE *f = fopen (argv[1], "rb");
   	int howMuch = rand() % 60 + 1;
  	int cnt = fread(m.payload, 1, howMuch, f);
  	char nr = 0;
	m.checksum = sum(m.payload, cnt); 
	m.nr = nr;
  	t.len = cnt + 2;
  	memcpy(t.payload, &m, sizeof(m));
  	while(cnt > 0) { 
    		assert(send_message(&t) != -1);
    		msg *x = receive_message_timeout(300);
    		char recv_nr;
    		if(x) {
      			r = *x;
      			memcpy(&recv_nr, r.payload, 1);
      			if(recv_nr == nr) {
        			howMuch = rand() % 60 + 1;
        			cnt = fread(m.payload, 1, howMuch, f);
        			t.len = cnt + 2;
        			memcpy(t.payload, &m, sizeof(m));
        			m.checksum = sum(m.payload, cnt);
        			nr++;
      			}
    		}
  	}
	t.len = -1;
  	send_message(&t);
  	fclose(f);
  	return 0;
}
