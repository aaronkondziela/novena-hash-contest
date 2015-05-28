#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/md5.h>
#include <errno.h>
#include <smmintrin.h>
#include <json/json.h>
#include <limits.h>
#include <time.h>
#include <linux/cuda.h>

unsigned char target[16] = {
	0x29, 0xc9, 0x86, 0xa4, 0x9a, 0xbf, 0x80, 0xe9,
	0xed, 0xf2, 0xff, 0xe8, 0xef, 0xb7, 0xe0, 0x40
};


#define UINT4 uint

extern __shared__ unsigned int words[];			// shared memory where hash will be stored
__constant__ unsigned int target_hash[4];		// constant has we will be searching for

__device__ unsigned int *format_shared_memory(unsigned int thread_id, unsigned int *memory) {
unsigned int *shared_memory;
unsigned int *global_memory;
int x;

	// we need to get a pointer to our shared memory portion

	shared_memory = &words[threadIdx.x * 16];
	global_memory = &memory[thread_id * 16];

	for(x=0; x < 16; x++) {
		shared_memory[x] = global_memory[x];
	}

	return shared_memory;
}



/* some utf8 validation ish from TidyLib */
/*
3) Legal UTF-8 byte sequences:
<http://www.unicode.org/unicode/uni2errata/UTF-8_Corrigendum.html>

Code point          1st byte    2nd byte    3rd byte    4th byte
----------          --------    --------    --------    --------
U+0000..U+007F      00..7F
U+0080..U+07FF      C2..DF      80..BF
U+0800..U+0FFF      E0          A0..BF      80..BF
U+1000..U+FFFF      E1..EF      80..BF      80..BF
U+10000..U+3FFFF    F0          90..BF      80..BF      80..BF
U+40000..U+FFFFF    F1..F3      80..BF      80..BF      80..BF
U+100000..U+10FFFF  F4          80..8F      80..BF      80..BF

The definition of UTF-8 in Annex D of ISO/IEC 10646-1:2000 also
allows for the use of five- and six-byte sequences to encode
characters that are outside the range of the Unicode character
set; those five- and six-byte sequences are illegal for the use
of UTF-8 as a transformation of Unicode characters. ISO/IEC 10646
does not allow mapping of unpaired surrogates, nor U+FFFE and U+FFFF
(but it does allow other noncharacters).
*/
#define kNumUTF8Sequences        7
#define kMaxUTF8Bytes            4
/* offsets into validUTF8 table below */
static const int offsetUTF8Sequences[kMaxUTF8Bytes + 1] =
{
	0, /* 1 byte */
	1, /* 2 bytes */
	2, /* 3 bytes */
	4, /* 4 bytes */
	kNumUTF8Sequences /* must be last */
};

static const struct validUTF8Sequence
{
	uint lowChar;
	uint highChar;
	int  numBytes;
	unsigned char validBytes[8];
} validUTF8[kNumUTF8Sequences] =
{
/*    low       high   #bytes  byte 1      byte 2      byte 3      byte 4 */
	{0x0000,   0x007F,   1, {0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x0080,   0x07FF,   2, {0xC2, 0xDF, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00}},
	{0x0800,   0x0FFF,   3, {0xE0, 0xE0, 0xA0, 0xBF, 0x80, 0xBF, 0x00, 0x00}},
	{0x1000,   0xFFFF,   3, {0xE1, 0xEF, 0x80, 0xBF, 0x80, 0xBF, 0x00, 0x00}},
	{0x10000,  0x3FFFF,  4, {0xF0, 0xF0, 0x90, 0xBF, 0x80, 0xBF, 0x80, 0xBF}},
	{0x40000,  0xFFFFF,  4, {0xF1, 0xF3, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF}},
	{0x100000, 0x10FFFF, 4, {0xF4, 0xF4, 0x80, 0x8F, 0x80, 0xBF, 0x80, 0xBF}} 
};

/*
UTF-8 is a specific scheme for mapping a sequence of 1-4 bytes to a number from 0x000000 to 0x10FFFF:
00000000 -- 0000007F: 	0xxxxxxx
00000080 -- 000007FF: 	110xxxxx 10xxxxxx
00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/

#define INVALID 0
#define VALID 1

char isValidCodepoint(unsigned int c)
{
	int i = 0;
	while(i++ < kNumUTF8Sequences) {
		if( c >= validUTF8[i].lowChar && c <= validUTF8[i].highChar) {
			return VALID;
		}
	}
	return INVALID;
}

// TODO char isValidUnicode( TODO

char isUnprintable(unsigned int a) {
	if( a <= 0x1f || a == 0x7f ) return 1; // C0
	if( a >= 0x80 && a <= 0x9f ) return 1; // C1
	if( a == 0x2028 || a == 0x2029 ) return 1; // line sep
	if( a >= 0xE0000 && a <= 0xE00FF ) return 1; // language tags (uncertain)
	if( a >= 0xFFF0 && a <= 0xFFFF ) return 1; // Specials, interlinear annotation
	if( a == 0x200E || a == 0x200F ) return 1; // bidir
	if( a >= 0x202A && a <= 0x202E ) return 1; // bidir
	if( a > 0x10FFFF ) return 1; // don't think they go higher...
	return 0;
}

char isTooLong(unsigned int a, int len) {
	switch(len) {
		case 1:
			if( a < 0x80 ) return 0;
			break;
		case 2:
			if( a < 0x800 ) return 0;
			break;
		case 3:
			if( a < 0x10000 ) return 0;
			break;
		case 4:
			if( a < 0x1FFFFF ) return 0;
			break;
		default:
			return 0;
	}
	return 1;
}

void genUTF8(unsigned char *s, int maxlen)
{
	//unsigned char s[65];
	unsigned int a;
	memset(s,0,maxlen);
	int current = 0;
	int howManyMore = 0;
	maxlen--; // room for trailing \0
	while( current < maxlen ) {
		howManyMore = maxlen - current;
		do {
			a = (rand() >> 10) & 0x0010FFFF; // mask off to make valid code point range. Shift gets us more hits
		} while( isTooLong(a,howManyMore) || isUnprintable(a) );

/*    low       high   #bytes  byte 1      byte 2      byte 3      byte 4
	{0x0000,   0x007F,   1, {0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x0080,   0x07FF,   2, {0xC2, 0xDF, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00}},
	{0x0800,   0x0FFF,   3, {0xE0, 0xE0, 0xA0, 0xBF, 0x80, 0xBF, 0x00, 0x00}},
	{0x1000,   0xFFFF,   3, {0xE1, 0xEF, 0x80, 0xBF, 0x80, 0xBF, 0x00, 0x00}},
	{0x10000,  0x3FFFF,  4, {0xF0, 0xF0, 0x90, 0xBF, 0x80, 0xBF, 0x80, 0xBF}},
	{0x40000,  0xFFFFF,  4, {0xF1, 0xF3, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF}},
	{0x100000, 0x10FFFF, 4, {0xF4, 0xF4, 0x80, 0x8F, 0x80, 0xBF, 0x80, 0xBF}}
00000000 -- 0000007F: 	0xxxxxxx
00000080 -- 000007FF: 	110xxxxx 10xxxxxx
00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
		if( a <= 0x7F ) {
			s[current++] = (unsigned char) a;
		} else if( a <= 0x7ff ) {
			s[current++] = (unsigned char) 0b11000000 | (a >> 6);
			s[current++] = (unsigned char) 0b10000000 | (a & 0b00111111);
		} else if( a <= 0xffff ) {
			s[current++] = (unsigned char) 0b11100000 | (a >> 12);
			s[current++] = (unsigned char) 0b10000000 | ((a >> 6) & 0b00111111);
			s[current++] = (unsigned char) 0b10000000 | (a & 0b00111111);
		} else if( a <= 0x1FFFFF ) { //expand range
			s[current++] = (unsigned char) 0b11110000 | (a >> 18);
			s[current++] = (unsigned char) 0b10000000 | ((a >> 12) & 0b00111111);
			s[current++] = (unsigned char) 0b10000000 | ((a >> 6) & 0b00111111);
			s[current++] = (unsigned char) 0b10000000 | (a & 0b00111111); 
		} else if( a >= 0x40000 && a <= 0xFFFFF ) {
			// encode
		} else if( a >= 0x100000 && a <= 0x10FFFF ) {
			// encode
		} else {
			printf("Error in unicode gen, var a out of range\n");
			exit(1);
		}
	}
	s[current] = '\0';
}

void getRandomBytes( unsigned char * towhere, int howmany )
{
	// howmany must be mul of 4
	if( howmany < 4 ) return;
	if( howmany % 4 != 0 ) return;
	int a = howmany / 4;
	while( a-- > 0 ) {
		( ((int *)towhere)[a] ) = rand();
	}
}

inline int fastcheck( unsigned char * hash, unsigned char * target, char difficulty )
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
	printf("Digest: %s\n", mdString);
}

void print_hexstring(unsigned char * s)
{
	int i;
	char hstring[130];
	hstring[128] = '\0';
	for(i=0; i<64; i++)
		sprintf(&hstring[i*2], "%02x", (unsigned int)s[i]);
	printf("String: %s\n", hstring);
}

void print_json(char * s)
{
	json_object * jobj = json_object_new_object();
	json_object * jstr;
	json_object * jstr2;
	jstr = json_object_new_string("aaronkondziela");
	json_object_object_add(jobj, "username", jstr);
	jstr2 = json_object_new_string(s);
	json_object_object_add(jobj, "contents", jstr2);
	printf("%s\n", json_object_to_json_string(jobj));
	json_object_put(jstr);
	json_object_put(jstr2);
	json_object_put(jobj);
}

int main(int argc, char ** argv) {
	unsigned char digest[16];
	unsigned char string[85];
	int urand;
	unsigned int iterations = UINT_MAX;
	int difficulty = 107;
	int c;
	while( (c = getopt(argc, argv, "i:d:")) != -1 ) {
		switch(c) {
			case 'i':
				sscanf(optarg, "%d", &iterations);
				break;
			case 'd':
				sscanf(optarg, "%d", &difficulty);
				break;
			default:
				break;
		}
	}

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

	memset( string, 0, 65 );
	memcpy( string, "novena\0", 7 );
	MD5_CTX ctx;

	unsigned int iter = 0;
	int x;
	while( iter++ < iterations ) {
		//getRandomBytes( string+6, 48 );
		genUTF8( string, rand() % 64 );
		MD5_Init(&ctx);
		MD5_Update(&ctx, string, 64);
		MD5_Final(digest, &ctx);
		if( (x = fastcheck(digest, target, difficulty)) ) {
			//print_digest(digest);
			printf("Found %d bitz are same\n", x);
			print_json((char *)string);
			print_hexstring(string);
		}
	}

	return 0;
}
