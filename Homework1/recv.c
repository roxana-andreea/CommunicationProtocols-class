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
	msg r, *p;
	init(HOST,PORT); 
	FILE *f = fopen(argv[1], "wb");
  	char expected = 0;
  	mesaj m;
	while (1) {
    		p = receive_message();
		if(p) 
      			r = *p;
    		else 
      			return -1;
		m = *(mesaj*)r.payload;
		if(r.len == -1)
      			break;
		if(m.nr == expected && m.checksum == sum(m.payload, r.len - 2)) {
      			fwrite(m.payload, 1, r.len - 2, f);
      			fflush(f);
      			expected++; 
    		}
		r.payload[0] = expected;
    		r.len = 1;
    		assert(send_message(&r));   
  	}
  	fclose(f);
  	return 0;
}
