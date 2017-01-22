#include <sstream>

#include "cryptopp/rsa.h"
#include "cryptopp/osrng.h"
#include "cryptopp/base64.h"
#include "cryptopp/files.h"

#include "crypto.h"

// global rng
static CryptoPP::AutoSeededRandomPool prng;

/* ------------------------------------------------------------------------ */
// General helper funcs

// string copy with null terminated safety
static size_t LegacyStrcpySafe( const std::string &from, char *to, size_t toLen ) {
	size_t copied = from.copy( to, toLen - 1 );
	to[copied] = '\0';
	return copied;
}

static bool FileExists( const char *fileName ) {
	std::ifstream file( fileName );
	return file.good();
}

/* ------------------------------------------------------------------------ */

#if 0
static void TestKeys( void ) {
#if 0
	std::string sourcePrivKeyStr;
	std::string sourcePubKeyStr;

	CryptoPP::RSAES_OAEP_SHA_Decryptor sourcePrivKey( prng, RSA_KEY_SIZE * 8 );
	CryptoPP::RSAES_OAEP_SHA_Encryptor sourcePubKey( sourcePrivKey );

	CryptoPP::Base64URLEncoder sourcePrivKeyStrTransform( new CryptoPP::StringSink( sourcePrivKeyStr ), false );
	CryptoPP::Base64URLEncoder sourcePubKeyStrTransform( new CryptoPP::StringSink( sourcePubKeyStr ), false );

	sourcePrivKey.DEREncode( sourcePrivKeyStrTransform );
	sourcePubKey.DEREncode( sourcePubKeyStrTransform );

#if 1
	CryptoPP::ByteQueue privQueue;
	CryptoPP::ByteQueue pubQueue;

	sourcePrivKey.DEREncode( privQueue );
	sourcePubKey.DEREncode( pubQueue );
#endif
#endif

#if 1
	std::string sourcePrivKeyStr = "MIIDhgIBADANBgkqhkiG9w0BAQEFAASCA3AwggNsAgEAAoG-ALlqZSEOBv0UsVtfiSkFhvZzJgLaK3Qmb5XlKRm62-1iRuX5E_lm8Cr0T33S26AaJtsJ_DWNz3BeE2lPgEMVPzr7XkgsJHI0V7zeGOerv4P86ZaBTCeIUhHU5jhd0Br4-NcEOpqAaRDNHMl58WhqWXrmWxCYIpoCqnmHvnjqaNsG8JR6yHV58XzqsKdxNbdL9ev-5-IEYZqcUhUvbH76VVaRhUxV2NmnM53JSmN5VqfUGfBMDYh7o5NB5doNJQIBEQKBvRMWPx3Bcakce6tJ1aYa7ALIFryHZlsD9OYExDseh5Sr_8TZo_BDETGZJkzz0tgCsS0i6QWDTdMYvjvILxX25KBssx4Ei0f929r4vsyN6k2WRTjnqbjGgOs_U-821WwtaPRYe-40RtN4l2r_4jJhEPuHc9XIMnhkegQ3vdgzlL8hXwShkKG1pwKf4bwR3whq5g_UpntaciwVZHMLOfeHZh6Rb5RrCmMM889MvWWS6nNB5Gr3GuSFO1nDfcDeeQJfDOJsaAAFXxTmkvAix11-DqXjkjOhW6AxwwTmcMnqqtUPeNRGC6S6rGFVujURkMbytvZMzaN02Wp1H91-rC0OPXTwmJJV6pLTqBM-xESREiCIfx_aWy0jlY4JwCeTLEkCXw5kAGU9iyV_kaa24GqQ3GOg51TmwDYF1ri7IPl3NW8TZknu_N_v4Q2C6R9YcumvBRjXBWiqCAV9eT7t780MVVBuGHQkwltzaFy7XK3V9691LWcqmSK2lGoeIYRmYwH9Al8MIGYHh4yVuVF7PFz334WzbvRNXcUK8R_GmzM8-kZGbiyt9Pahi_r8l9g2yIkAuz7KUTk6A0DMoG48G8KD7iuFIsRTXI0ZAqkWqLOLi9PU01NKeFUKhNYyZ5C02fPtcQJfBRQ8X_eahbSNwl6pjwXzbnUGWjNS5ePxbmApwXVeJzQF-_n-5Z_1E9PZztPsUnoBzIggBtKZa1l2FjXcKkCWlNuQKP3qPmT3qEIgtdMMH88A9zw2DEBwneyETNjXplkCXwbcWaUp1N6NqW5oE1HcIMOwtJQsb1VxfGG2_xb6PXTOMdrbOz5BjATJ2YcPgzj_gHptw9LTN5eev9Azul4M6Cyl7zPRt5jnc-K15-KHZlb5slFc2tqJNkKxuzIOzsZ9";
	std::string sourcePubKeyStr = "MIHaMA0GCSqGSIb3DQEBAQUAA4HIADCBxAKBvgC5amUhDgb9FLFbX4kpBYb2cyYC2it0Jm-V5SkZutvtYkbl-RP5ZvAq9E990tugGibbCfw1jc9wXhNpT4BDFT86-15ILCRyNFe83hjnq7-D_OmWgUwniFIR1OY4XdAa-PjXBDqagGkQzRzJefFoall65lsQmCKaAqp5h7546mjbBvCUesh1efF86rCncTW3S_Xr_ufiBGGanFIVL2x--lVWkYVMVdjZpzOdyUpjeVan1BnwTA2Ie6OTQeXaDSUCARE";
#endif

	// ----------------------------------------------------------------

#if 0
	CryptoPP::Base64URLDecoder privSaver( new CryptoPP::FileSink( "priv.der", true ) );
	CryptoPP::Base64URLDecoder pubSaver( new CryptoPP::FileSink( "pub.der", true ) );

	privSaver.Put( ( const byte* )sourcePrivKeyStr.data(), sourcePrivKeyStr.size() );
	pubSaver.Put( ( const byte* )sourcePubKeyStr.data(), sourcePubKeyStr.size() );

	privSaver.MessageEnd();
	pubSaver.MessageEnd();
#endif

	// ----------------------------------------------------------------

	CryptoPP::RSAES_OAEP_SHA_Decryptor targPrivKey;
	CryptoPP::RSAES_OAEP_SHA_Encryptor targPubKey;

#if 1
	CryptoPP::StringSource privKeyTransform( sourcePrivKeyStr, true, new CryptoPP::Base64URLDecoder );
	CryptoPP::StringSource pubKeyTransform( sourcePubKeyStr, true, new CryptoPP::Base64URLDecoder );

	targPrivKey.BERDecode( privKeyTransform );
	targPubKey.BERDecode( pubKeyTransform );
#endif

#if 0
	CryptoPP::ByteQueue privQueue;
	CryptoPP::ByteQueue pubQueue;

	CryptoPP::StringSource privKeyTransform( sourcePrivKeyStr, true, new CryptoPP::Base64URLDecoder( new CryptoPP::Redirector( privQueue ) ) );
	CryptoPP::StringSource pubKeyTransform( sourcePubKeyStr, true, new CryptoPP::Base64URLDecoder( new CryptoPP::Redirector( pubQueue ) ) );
#endif

#if 0
	CryptoPP::ByteQueue privQueue;
	CryptoPP::ByteQueue pubQueue;

	CryptoPP::Base64URLDecoder privKeyTransform( new CryptoPP::Redirector( privQueue ) );
	CryptoPP::Base64URLDecoder pubKeyTransform( new CryptoPP::Redirector( pubQueue ) );

	privKeyTransform.Put( ( const byte* )sourcePrivKeyStr.data(), sourcePrivKeyStr.size() );
	pubKeyTransform.Put( ( const byte* )sourcePubKeyStr.data(), sourcePubKeyStr.size() );

	privKeyTransform.MessageEnd();
	pubKeyTransform.MessageEnd();
#endif

#if 0
	CryptoPP::FileSink privQueueSaver( "privqueue_dmp.dat" );
	CryptoPP::FileSink pubQueueSaver( "pubqueue_dmp.dat" );

	privQueue.CopyTo( privQueueSaver );
	pubQueue.CopyTo( pubQueueSaver );

	privQueueSaver.MessageEnd();
	pubQueueSaver.MessageEnd();
#endif

#if 0
	targPrivKey.BERDecode( privQueue );
	targPubKey.BERDecode( pubQueue );
#endif

	std::string targPrivKeyStr;
	std::string targPubKeyStr;

	CryptoPP::Base64URLEncoder targPrivKeyStrTransform( new CryptoPP::StringSink( targPrivKeyStr ), false );
	CryptoPP::Base64URLEncoder targPubKeyStrTransform( new CryptoPP::StringSink( targPubKeyStr ), false );

	targPrivKey.DEREncode( targPrivKeyStrTransform );
	targPubKey.DEREncode( targPubKeyStrTransform );

#if 1
	CryptoPP::ByteQueue getraped;

	targPubKey.DEREncode( getraped );

	std::string rapedString;

	CryptoPP::Base64URLEncoder rapedTransform( new CryptoPP::StringSink( rapedString ), false );
	getraped.CopyTo( rapedTransform );
	rapedTransform.MessageEnd();
#endif
}
#endif

/* ------------------------------------------------------------------------ */
// Helper funcs needed to work around a crypto++ bug with public keys omitting
// the last bytes when piped into a non ByteQueue transformation

static void SaveKeyTo( CryptoPP::AsymmetricAlgorithm &key, CryptoPP::BufferedTransformation &out ) {
	CryptoPP::ByteQueue queue;
	key.DEREncode( queue );
	queue.CopyTo( out );
	out.MessageEnd();
}

static void LoadKeyFrom( CryptoPP::AsymmetricAlgorithm &key, CryptoPP::BufferedTransformation &in ) {
	CryptoPP::ByteQueue queue;
	in.TransferTo( queue );
	queue.MessageEnd();
	key.BERDecode( queue );
}

/* ------------------------------------------------------------------------ */

// loads a DER/ASN.1 encoded key from disk ; generates the file if it does not exist
// you have to instruct the function on how to create the key: if you are creating a public key, also
// provide its parent private key that will be used in the callback
template <class T>
static std::unique_ptr<T> LoadOrGenerateKey( const char *fileName, CryptoPP::RandomNumberGenerator &rng, CryptoPP::AsymmetricAlgorithm *parentKey,
	std::unique_ptr<T>( *MakeCallback )( CryptoPP::RandomNumberGenerator &rng, CryptoPP::AsymmetricAlgorithm *parentKey ) ) {
	std::unique_ptr<T> result;	

	if ( !FileExists( fileName ) ) {
		// generate a random key and write it to disk
		result = std::move( MakeCallback( rng, parentKey ) );

		CryptoPP::FileSink outFile( fileName, true );
		SaveKeyTo( *result, outFile );
	} else {
		// read the key from disk
		result.reset( new T );

		CryptoPP::FileSource inFile( fileName, true );
		LoadKeyFrom( *result, inFile );
	}

	return result;
}

// loads a base64url DER/ASN.1 encoded key
template <class T>
static std::unique_ptr<T> LoadKeyFromBase64Url( const char *keyStrBase64Url ) {
	std::unique_ptr<T> result( new T );

	CryptoPP::StringSource inDecoder( keyStrBase64Url, true, new CryptoPP::Base64URLDecoder );
	LoadKeyFrom( *result, inDecoder );

	return result;
}

/* ------------------------------------------------------------------------ */
// RSA implementation

static std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Decryptor> rsaPriv;
static std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Encryptor> rsaPub;

// loads and validates keys in local memory for use restricted to this file, depending on the parameters:
// - with NULL, the key will be loaded from disk if it exists, or randomly generated otherwise
// - with an empty string, the key will not be loaded but associated functions will fail
// - with a valid base64url DER/ASN.1 string, the key will be loaded from that string
static void InitRSA( const char *pubKeyStrBase64Url = nullptr, const char *privKeyStrBase64Url = nullptr ) {
	if ( privKeyStrBase64Url && *privKeyStrBase64Url ) {
		rsaPriv = std::move( LoadKeyFromBase64Url<CryptoPP::RSAES_OAEP_SHA_Decryptor>( privKeyStrBase64Url ) );
	} else if ( !privKeyStrBase64Url ) {
		rsaPriv = std::move( LoadOrGenerateKey<CryptoPP::RSAES_OAEP_SHA_Decryptor>( RSA_PRIVKEY_FILE, prng, nullptr,
			[]( CryptoPP::RandomNumberGenerator &rng, CryptoPP::AsymmetricAlgorithm* ) {
				std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Decryptor> privateKey( new CryptoPP::RSAES_OAEP_SHA_Decryptor( rng, RSA_KEY_SIZE ) );
				return privateKey;
			}
		) );
	}

	if ( rsaPriv ) {
		// we loaded a private key, validate it
		unsigned int keySize = ( *rsaPriv ).AccessKey().GetModulus().BitCount();
		if ( keySize != RSA_KEY_SIZE ) {
			throw std::runtime_error( std::string( "Private key has invalid size " ) + std::to_string( keySize ) + " (expected " + std::to_string( RSA_KEY_SIZE ) + ")" );
		}

		if ( !( *rsaPriv ).GetMaterial().Validate( prng, RSA_VALIDATE_LEVEL ) ) {
			throw std::runtime_error( "Failed to validate private key" );
		}
	}

	if ( pubKeyStrBase64Url && *pubKeyStrBase64Url ) {
		rsaPub = std::move( LoadKeyFromBase64Url<CryptoPP::RSAES_OAEP_SHA_Encryptor>( pubKeyStrBase64Url ) );
	} else if ( !pubKeyStrBase64Url ) {
		rsaPub = std::move( LoadOrGenerateKey<CryptoPP::RSAES_OAEP_SHA_Encryptor>( RSA_PUBKEY_FILE, prng, &( *rsaPriv ),
			[]( CryptoPP::RandomNumberGenerator&, CryptoPP::AsymmetricAlgorithm *parentKey ) {
				std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Encryptor> publicKey( new CryptoPP::RSAES_OAEP_SHA_Encryptor( *parentKey ) );
				return publicKey;
			}
		) );
	}

	if ( rsaPub ) {
		// we loaded a public key, validate it
		unsigned int keySize = ( *rsaPub ).AccessKey().GetModulus().BitCount();
		if ( keySize != RSA_KEY_SIZE ) {
			throw std::runtime_error( std::string( "Public key has invalid size " ) + std::to_string( keySize ) + " (expected " + std::to_string( RSA_KEY_SIZE ) + ")" );
		}

		if ( !( *rsaPub ).GetMaterial().Validate( prng, RSA_VALIDATE_LEVEL ) ) {
			throw std::runtime_error( "Failed to validate public key" );
		}
	}
}

static void FreeRSA( void ) {
	rsaPriv.reset( nullptr );
	rsaPub.reset( nullptr );
}

static std::string DumpKeyBase64Url( CryptoPP::AsymmetricAlgorithm &key ) {
	std::string result;
	CryptoPP::Base64URLEncoder outEncoder( new CryptoPP::StringSink( result ), false );
	SaveKeyTo( key, outEncoder );

	return result;
}

// throws an exception if public key is not initialized
static std::string EncryptStringBase64Url( const char *in ) {
	if ( !rsaPub ) {
		throw std::runtime_error( "Public key is not initialized" );
	}

	std::string out;
	CryptoPP::StringSource( in, true, new CryptoPP::PK_EncryptorFilter( prng, *rsaPub, new CryptoPP::Base64URLEncoder( new CryptoPP::StringSink( out ), false ) ) );

	return out;
}

// throws an exception if private key is not initialized
static std::string DecryptStringBase64Url( const char *in ) {
	if ( !rsaPriv ) {
		throw std::runtime_error( "Private key is not initialized" );
	}

	std::string out;
	CryptoPP::StringSource( in, true, new CryptoPP::Base64URLDecoder( new CryptoPP::PK_DecryptorFilter( prng, *rsaPriv, new CryptoPP::StringSink( out ) ) ) );

	return out;
}

/* ------------------------------------------------------------------------ */

static std::stringstream lastError;

// returns CRYPTO_ERROR for convenience when returning from catch blocks
static int SetLastError( const std::exception &e, const char *function = "" ) {
	lastError.clear();
	lastError << "Exception caught";
	if ( *function ) lastError << "in " << function;
	lastError << ": " << e.what();

	return CRYPTO_ERROR;
}

/* ------------------------------------------------------------------------ */
// Type safe wrappers, redirects all exceptions to a compatible error stream

// inits the RSA system using locally stored keys which are generated the first time
int Crypto_RSAInit( void ) {
	try {
		InitRSA();
	} catch ( const std::exception &e ) {
		FreeRSA();
		return SetLastError( e, __FUNCTION__ );
	}

	return CRYPTO_FINE;
}

// inits the RSA system by providing keys instead, use NULL to skip a key
int Crypto_RSAInitWithB64UKeys( const char *pubKeyStrBase64Url, const char *privKeyStrBase64Url ) {
	try {
		InitRSA( pubKeyStrBase64Url, privKeyStrBase64Url );
	} catch ( const std::exception &e ) {
		FreeRSA();
		return SetLastError( e, __FUNCTION__ );
	}

	return CRYPTO_FINE;
}

// explicitly call the destroyers on locally stored RSA objects
void Crypto_RSAFree( void ) {
	FreeRSA();
}

// dumps a key in base64url format to the provided buffer
// returns the amount of chars written if successful
int Crypto_RSADumpKey( int pub, char *out, size_t outLen ) {
	out[0] = '\0'; // in case of failure, empty result

	try {
		if ( pub && !rsaPub ) {
			throw std::runtime_error( "Public key is not initialized" );
		} else if ( !pub && !rsaPriv ) {
			throw std::runtime_error( "Private key is not initialized" );
		}

		return pub ? LegacyStrcpySafe( DumpKeyBase64Url( *rsaPub ), out, outLen )
			: LegacyStrcpySafe( DumpKeyBase64Url( *rsaPriv ), out, outLen );
	} catch ( std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}
}

// encrypts the provided buffer with the locally stored public key
// returns the amount of chars written if successful
int Crypto_RSAEncrypt( const char *in, char *out, size_t outLen ) {
	out[0] = '\0'; // in case of failure, empty result

	try {
		return LegacyStrcpySafe( EncryptStringBase64Url( in ), out, outLen );
	} catch ( const std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}
}

// decrypts the provided buffer with the locally stored private key
// returns the amount of chars written if successful
int Crypto_RSADecrypt( const char *in, char *out, size_t outLen ) {
	out[0] = '\0'; // in case of failure, empty result

	try {
		return LegacyStrcpySafe( DecryptStringBase64Url( in ), out, outLen );
	} catch ( const std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}
}

// whenever a function returns CRYPTO_ERROR, use this for the error message
const char* Crypto_LastError( void ) {
	return lastError.str().c_str();
}

/* ------------------------------------------------------------------------ */