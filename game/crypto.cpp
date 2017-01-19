#include <sstream>

#include "cryptopp/rsa.h"
#include "cryptopp/osrng.h"
#include "cryptopp/hex.h"
#include "cryptopp/base64.h"
#include "cryptopp/files.h"

#include "crypto.h"

// global rng
static CryptoPP::AutoSeededRandomPool prng;

static std::stringstream lastError;

const char* Crypto_LastError( void ) {
	return lastError.str().c_str();
}

// returns CRYPTO_ERROR for convenience when returning from catch blocks
static int SetLastError( const std::exception &e, const char *function = "" ) {
	lastError.clear();
	lastError << "Exception caught";
	if ( *function ) lastError << "in " << function;
	lastError << ": " << e.what();

	return CRYPTO_ERROR;
}

static size_t LegacyStrcpySafe( const std::string &from, char *to, size_t toLen ) {
	size_t copied = from.copy( to, toLen - 1 );
	to[copied] = '\0';
	return copied;
}

static bool FileExists( const char *fileName ) {
	std::ifstream file( fileName );
	return file.good();
}

static std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Decryptor> rsaPriv;
static std::unique_ptr<CryptoPP::RSAES_OAEP_SHA_Encryptor> rsaPub;

static void LoadOrGenerateKey( const char *fileName, bool pub, CryptoPP::RandomNumberGenerator &rng ) {
	if ( !FileExists( fileName ) ) {
		// generate a random key and write it to disk
		CryptoPP::HexEncoder outFile( new CryptoPP::FileSink( fileName ) );

		if ( pub ) {
			// rsaPriv must already be initialized
			rsaPub.reset( new CryptoPP::RSAES_OAEP_SHA_Encryptor( *rsaPriv ) );
			( *rsaPub ).DEREncode( outFile );
		} else {
			rsaPriv.reset( new CryptoPP::RSAES_OAEP_SHA_Decryptor( rng, RSA_KEY_SIZE * 8 ) );
			( *rsaPriv ).DEREncode( outFile );
		}

		outFile.MessageEnd();
	} else {
		// read the file from disk
		CryptoPP::FileSource inFile( fileName, true, new CryptoPP::HexDecoder );

		if ( pub ) {
			rsaPub.reset( new CryptoPP::RSAES_OAEP_SHA_Encryptor( inFile ) );
		} else {
			rsaPriv.reset( new CryptoPP::RSAES_OAEP_SHA_Decryptor( inFile ) );
		}
	}

	// make sure the key is usable
	if ( pub && !( *rsaPub ).GetMaterial().Validate( rng, RSA_VALIDATE_LEVEL ) ) {
		throw std::runtime_error( "Failed to validate public key" );
	} else if ( !pub && !( *rsaPriv ).GetMaterial().Validate( rng, RSA_VALIDATE_LEVEL ) ) {
		throw std::runtime_error( "Failed to validate private key" );
	}
}

int Crypto_RSAInit( void ) {
	try {
		LoadOrGenerateKey( "private.key", false, prng );
		LoadOrGenerateKey( "public.key", true, prng );
	} catch ( const std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}

	return CRYPTO_FINE;
}

void Crypto_RSAFree( void ) {
	rsaPriv.reset( nullptr );
	rsaPub.reset( nullptr );
}

int Crypto_RSADumpKey( int pub, char *out, size_t outLen ) {
	std::string result;
	out[0] = '\0'; // in case of failure, empty result

	try {
		CryptoPP::Base64Encoder outTransform( new CryptoPP::StringSink( result ), false );

		if ( pub ) {
			( *rsaPub ).DEREncode( outTransform );
		} else {
			( *rsaPriv ).DEREncode( outTransform );
		}
	} catch ( std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}

	return LegacyStrcpySafe( result, out, outLen );
}

static void RSAEncryptBase64( const char *in, std::string &out ) {
	CryptoPP::StringSource( in, true, new CryptoPP::PK_EncryptorFilter( prng, *rsaPub, new CryptoPP::Base64Encoder( new CryptoPP::StringSink( out ), false ) ) );
}

static void RSADecryptBase64( const char *in, std::string &out ) {
	CryptoPP::StringSource( in, true, new CryptoPP::Base64Decoder( new CryptoPP::PK_DecryptorFilter( prng, *rsaPriv, new CryptoPP::StringSink( out ) ) ) );
}

static size_t TransformLegacyBufferSafe( const char *in, char *out, size_t outLen, void( *Callback )( const char*, std::string& ) ) {
	std::string result;
	out[0] = '\0'; // in case of failure, empty result

	Callback( in, result );

	return LegacyStrcpySafe( result, out, outLen );
}

int Crypto_RSAEncrypt( const char *in, char *out, size_t outLen ) {
	try {
		return TransformLegacyBufferSafe( in, out, outLen, RSAEncryptBase64 );
	} catch ( const std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}
}

int Crypto_RSADecrypt( const char *in, char *out, size_t outLen ) {
	try {
		return TransformLegacyBufferSafe( in, out, outLen, RSADecryptBase64 );
	} catch ( const std::exception &e ) {
		return SetLastError( e, __FUNCTION__ );
	}
}