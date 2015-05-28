// (c)2014-2015 Aaron Kondziela <aaron@aaronkondziela.com>
// Distributed under the MIT License

#include <stdio.h>
#include <stdlib.h>
#include <smmintrin.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

unsigned char target[16] = {
        0x29, 0xc9, 0x86, 0xa4, 0x9a, 0xbf, 0x80, 0xe9,
        0xed, 0xf2, 0xff, 0xe8, 0xef, 0xb7, 0xe0, 0x40
};

inline int countbits( unsigned char * hash, unsigned char * target, int difficulty )
{
        int c;
        // Ghetto-fast 128-bit math
        c = _mm_popcnt_u64( ~ (*(unsigned long *)hash ^ *(unsigned long *)target) ) +
                _mm_popcnt_u64( ~ (*(unsigned long *)(hash+8) ^ *(unsigned long *)(target+8)) );
        //printf("Found %d bitz are same\n", c);
        if( c >= difficulty ) {
                return(c);
        } else {
                return(0);
        }
}

void print_digest(unsigned char * digest)
{
        int i;
        char mdString[33];
        mdString[32] = '\0';
        for (i = 0; i < 16; i++)
                sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
        printf("%s\n", mdString);
}

int main(void)
{
	unsigned char h[16];
	unsigned char bitstoflip = 0;
	unsigned char byte = 0;
	//unsigned int c = UINT_MAX;
	unsigned int c = 40000000;
	//unsigned int c = 3;
	int urand;

        urand = open("/dev/urandom", O_RDONLY);
        if( urand == -1 ) {
                printf("error opening urand\n");
                exit(1);
        }
        int seed;
        if( read(urand, &seed, sizeof(seed)) != sizeof(seed)) {
                printf("Error reading urandom\n");
                exit(2);
        }
        srand(seed);
        close(urand);
	
	while(c--) {
		memcpy( h, target, 16 );
		bitstoflip = rand() % 22;
		while( bitstoflip-- ) {
			byte = rand() % 16;
			h[byte] ^= (1 << (rand() % 8));
		}
		if( countbits( h, target, 107 ) ) {
			print_digest(h);
		}
	}
	return(0);
}
