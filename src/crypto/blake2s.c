/**
 * BLAKE2 reference source code package - reference C implementations
 *
 * Written in 2012 by Samuel Neves <sneves@dei.uc.pt>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along with
 * this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "crypto/sph_types.h"
#include "crypto/blake2s.h"

static const uint32_t myblake2s_IV[8] =
{
	0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t myblake2s_sigma[10][16] =
{
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
};

static inline int myblake2s_set_lastnode( myblake2s_state *S )
{
	S->f[1] = ~0U;
	return 0;
}

static inline int myblake2s_clear_lastnode( myblake2s_state *S )
{
	S->f[1] = 0U;
	return 0;
}

/* Some helper functions, not necessarily useful */
static inline int myblake2s_set_lastblock( myblake2s_state *S )
{
	if( S->last_node ) myblake2s_set_lastnode( S );

	S->f[0] = ~0U;
	return 0;
}

static inline int myblake2s_clear_lastblock( myblake2s_state *S )
{
	if( S->last_node ) myblake2s_clear_lastnode( S );

	S->f[0] = 0U;
	return 0;
}

static inline int myblake2s_increment_counter( myblake2s_state *S, const uint32_t inc )
{
	S->t[0] += inc;
	S->t[1] += ( S->t[0] < inc );
	return 0;
}

// Parameter-related functions
static inline int myblake2s_param_set_digest_length( myblake2s_param *P, const uint8_t digest_length )
{
	P->digest_length = digest_length;
	return 0;
}

static inline int myblake2s_param_set_fanout( myblake2s_param *P, const uint8_t fanout )
{
	P->fanout = fanout;
	return 0;
}

static inline int myblake2s_param_set_max_depth( myblake2s_param *P, const uint8_t depth )
{
	P->depth = depth;
	return 0;
}

static inline int myblake2s_param_set_leaf_length( myblake2s_param *P, const uint32_t leaf_length )
{
	store32( &P->leaf_length, leaf_length );
	return 0;
}

static inline int myblake2s_param_set_node_offset( myblake2s_param *P, const uint64_t node_offset )
{
	store48( P->node_offset, node_offset );
	return 0;
}

static inline int myblake2s_param_set_node_depth( myblake2s_param *P, const uint8_t node_depth )
{
	P->node_depth = node_depth;
	return 0;
}

static inline int myblake2s_param_set_inner_length( myblake2s_param *P, const uint8_t inner_length )
{
	P->inner_length = inner_length;
	return 0;
}

static inline int myblake2s_param_set_salt( myblake2s_param *P, const uint8_t salt[myblake2s_SALTBYTES] )
{
	memcpy( P->salt, salt, myblake2s_SALTBYTES );
	return 0;
}

static inline int myblake2s_param_set_personal( myblake2s_param *P, const uint8_t personal[myblake2s_PERSONALBYTES] )
{
	memcpy( P->personal, personal, myblake2s_PERSONALBYTES );
	return 0;
}

static inline int myblake2s_init0( myblake2s_state *S )
{
	memset( S, 0, sizeof( myblake2s_state ) );

	for( int i = 0; i < 8; ++i ) S->h[i] = myblake2s_IV[i];

	return 0;
}

/* init2 xors IV with input parameter block */
int myblake2s_init_param( myblake2s_state *S, const myblake2s_param *P )
{
	myblake2s_init0( S );
	uint32_t *p = ( uint32_t * )( P );

	/* IV XOR ParamBlock */
	for( size_t i = 0; i < 8; ++i )
		S->h[i] ^= load32( &p[i] );

	return 0;
}


// Sequential myblake2s initialization
int myblake2s_init( myblake2s_state *S, const uint8_t outlen )
{
	myblake2s_param P[1];

	/* Move interval verification here? */
	if ( ( !outlen ) || ( outlen > myblake2s_OUTBYTES ) ) return -1;

	P->digest_length = outlen;
	P->key_length    = 0;
	P->fanout        = 1;
	P->depth         = 1;
	store32( &P->leaf_length, 0 );
	store48( &P->node_offset, 0 );
	P->node_depth    = 0;
	P->inner_length  = 0;
	// memset(P->reserved, 0, sizeof(P->reserved) );
	memset( P->salt,     0, sizeof( P->salt ) );
	memset( P->personal, 0, sizeof( P->personal ) );
	return myblake2s_init_param( S, P );
}

int myblake2s_init_key( myblake2s_state *S, const uint8_t outlen, const void *key, const uint8_t keylen )
{
	myblake2s_param P[1];

	if ( ( !outlen ) || ( outlen > myblake2s_OUTBYTES ) ) return -1;

	if ( !key || !keylen || keylen > myblake2s_KEYBYTES ) return -1;

	P->digest_length = outlen;
	P->key_length    = keylen;
	P->fanout        = 1;
	P->depth         = 1;
	store32( &P->leaf_length, 0 );
	store48( &P->node_offset, 0 );
	P->node_depth    = 0;
	P->inner_length  = 0;
	// memset(P->reserved, 0, sizeof(P->reserved) );
	memset( P->salt,     0, sizeof( P->salt ) );
	memset( P->personal, 0, sizeof( P->personal ) );

	if( myblake2s_init_param( S, P ) < 0 ) return -1;

	{
		uint8_t block[myblake2s_BLOCKBYTES];
		memset( block, 0, myblake2s_BLOCKBYTES );
		memcpy( block, key, keylen );
		myblake2s_update( S, block, myblake2s_BLOCKBYTES );
		secure_zero_memory( block, myblake2s_BLOCKBYTES ); /* Burn the key from stack */
	}
	return 0;
}

int myblake2s_compress( myblake2s_state *S, const uint8_t block[myblake2s_BLOCKBYTES] )
{
	uint32_t m[16];
	uint32_t v[16];

	for( size_t i = 0; i < 16; ++i )
		m[i] = load32( block + i * sizeof( m[i] ) );

	for( size_t i = 0; i < 8; ++i )
		v[i] = S->h[i];

	v[ 8] = myblake2s_IV[0];
	v[ 9] = myblake2s_IV[1];
	v[10] = myblake2s_IV[2];
	v[11] = myblake2s_IV[3];
	v[12] = S->t[0] ^ myblake2s_IV[4];
	v[13] = S->t[1] ^ myblake2s_IV[5];
	v[14] = S->f[0] ^ myblake2s_IV[6];
	v[15] = S->f[1] ^ myblake2s_IV[7];
#define G(r,i,a,b,c,d) \
	do { \
		a = a + b + m[myblake2s_sigma[r][2*i+0]]; \
		d = SPH_ROTR32(d ^ a, 16); \
		c = c + d; \
		b = SPH_ROTR32(b ^ c, 12); \
		a = a + b + m[myblake2s_sigma[r][2*i+1]]; \
		d = SPH_ROTR32(d ^ a, 8); \
		c = c + d; \
		b = SPH_ROTR32(b ^ c, 7); \
	} while(0)
#define ROUND(r)  \
	do { \
		G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
		G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
		G(r,2,v[ 2],v[ 6],v[10],v[14]); \
		G(r,3,v[ 3],v[ 7],v[11],v[15]); \
		G(r,4,v[ 0],v[ 5],v[10],v[15]); \
		G(r,5,v[ 1],v[ 6],v[11],v[12]); \
		G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
		G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
	} while(0)
	ROUND( 0 );
	ROUND( 1 );
	ROUND( 2 );
	ROUND( 3 );
	ROUND( 4 );
	ROUND( 5 );
	ROUND( 6 );
	ROUND( 7 );
	ROUND( 8 );
	ROUND( 9 );

	for( size_t i = 0; i < 8; ++i )
		S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];

#undef G
#undef ROUND
	return 0;
}


int myblake2s_update( myblake2s_state *S, const uint8_t *in, uint64_t inlen )
{
	while( inlen > 0 )
	{
		size_t left = S->buflen;
		size_t fill = 2 * myblake2s_BLOCKBYTES - left;

		if( inlen > fill )
		{
			memcpy( S->buf + left, in, fill ); // Fill buffer
			S->buflen += fill;
			myblake2s_increment_counter( S, myblake2s_BLOCKBYTES );
			myblake2s_compress( S, S->buf ); // Compress
			memcpy( S->buf, S->buf + myblake2s_BLOCKBYTES, myblake2s_BLOCKBYTES ); // Shift buffer left
			S->buflen -= myblake2s_BLOCKBYTES;
			in += fill;
			inlen -= fill;
		}
		else // inlen <= fill
		{
			memcpy(S->buf + left, in, (size_t) inlen);
			S->buflen += (size_t) inlen; // Be lazy, do not compress
			in += inlen;
			inlen -= inlen;
		}
	}

	return 0;
}

int myblake2s_final( myblake2s_state *S, uint8_t *out, uint8_t outlen )
{
	uint8_t buffer[myblake2s_OUTBYTES];

	if( S->buflen > myblake2s_BLOCKBYTES )
	{
		myblake2s_increment_counter( S, myblake2s_BLOCKBYTES );
		myblake2s_compress( S, S->buf );
		S->buflen -= myblake2s_BLOCKBYTES;
		memcpy( S->buf, S->buf + myblake2s_BLOCKBYTES, S->buflen );
	}

	myblake2s_increment_counter( S, ( uint32_t )S->buflen );
	myblake2s_set_lastblock( S );
	memset( S->buf + S->buflen, 0, 2 * myblake2s_BLOCKBYTES - S->buflen ); /* Padding */
	myblake2s_compress( S, S->buf );

	for( int i = 0; i < 8; ++i ) /* Output full hash to temp buffer */
		store32( buffer + sizeof( S->h[i] ) * i, S->h[i] );

	memcpy( out, buffer, outlen );
	return 0;
}

int myblake2s( uint8_t *out, const void *in, const void *key, const uint8_t outlen, const uint64_t inlen, uint8_t keylen )
{
	myblake2s_state S[1];

	/* Verify parameters */
	if ( NULL == in ) return -1;

	if ( NULL == out ) return -1;

	if ( NULL == key ) keylen = 0; /* Fail here instead if keylen != 0 and key == NULL? */

	if( keylen > 0 )
	{
		if( myblake2s_init_key( S, outlen, key, keylen ) < 0 ) return -1;
	}
	else
	{
		if( myblake2s_init( S, outlen ) < 0 ) return -1;
	}

	myblake2s_update( S, ( uint8_t * )in, inlen );
	myblake2s_final( S, out, outlen );
	return 0;
}

#if defined(myblake2s_SELFTEST)
#include <string.h>
#include "blake2-kat.h" /* test data not included */
int main( int argc, char **argv )
{
	uint8_t key[myblake2s_KEYBYTES];
	uint8_t buf[KAT_LENGTH];

	for( size_t i = 0; i < myblake2s_KEYBYTES; ++i )
		key[i] = ( uint8_t )i;

	for( size_t i = 0; i < KAT_LENGTH; ++i )
		buf[i] = ( uint8_t )i;

	for( size_t i = 0; i < KAT_LENGTH; ++i )
	{
		uint8_t hash[myblake2s_OUTBYTES];
		myblake2s( hash, buf, key, myblake2s_OUTBYTES, i, myblake2s_KEYBYTES );

		if( 0 != memcmp( hash, myblake2s_keyed_kat[i], myblake2s_OUTBYTES ) )
		{
			puts( "error" );
			return -1;
		}
	}

	puts( "ok" );
	return 0;
}
#endif
