#ifndef __G_CRYPTO__
#define __G_LOCAL__

/* most functions return these codes depending on result */
#define CRYPTO_FINE				0
#define CRYPTO_ERROR			-1

#define RSA_VALIDATE_LEVEL		1						// 0 for no checks, 1 for functionality check, 2 for security check, 3 for full checks

#define RSA_KEY_BITS			189						// this is the largest key size that generates base64 output <= 255 chars
#define RSA_KEY_SIZE			RSA_KEY_BITS * 8

/* the values below must be computed according to RSA_KEY_BITS */
#define RSA_PUB_B64U_BITS		295						// the fixed length of a base64url pub key
#define RSA_MAX_PUB_B64U_CHARS	RSA_PUB_B64U_BITS + 1	// use this for base64url encoded pub keys buffers
#define RSA_UNPADDED_BITS		147						// the max length of a string to encrypt
#define RSA_MAX_DEC_CHARS		RSA_UNPADDED_BITS + 1	// use this for plain/decrypted buffers
#define RSA_ENC_B64U_BITS		252						// the fixed length of an encrypted base64url string
#define RSA_MAX_ENC_B64U_CHARS	RSA_ENC_B64U_BITS + 1	// use this for base64url encoded encrypted buffers

#define RSA_PUBKEY_FILE			"public.der"
#define RSA_PRIVKEY_FILE		"private.der"

#define HASH_SHA1_HEX_BITS		40
#define HASH_MAX_SHA1_HEX_CHARS	HASH_SHA1_HEX_BITS + 1	// use this for hex sha1 buffers

#ifdef __cplusplus
extern "C" {
#endif
	int Crypto_RSAInit( void );
	int Crypto_RSAInitWithB64UKeys( const char *pubKeyStrBase64Url, const char *privKeyStrBase64Url );
	void Crypto_RSAFree( void );
	int Crypto_RSADumpKey( int pub, char *out, size_t outLen );
	int Crypto_RSAEncrypt( const char *in, char *out, size_t outLen );
	int Crypto_RSADecrypt( const char *in, char *out, size_t outLen );
	int Crypto_Hash( const char *in, char *out, size_t outLen );
	const char* Crypto_LastError( void );
#ifdef __cplusplus
}
#endif

#endif // __G_CRYPTO__