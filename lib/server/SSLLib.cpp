// --------------------------------------------------------------------------
//
// File
//		Name:    SSLLib.cpp
//		Purpose: Utility functions for dealing with the OpenSSL library
//		Created: 2003/08/06
//
// --------------------------------------------------------------------------

#include "Box.h"

#define TLS_CLASS_IMPLEMENTATION_CPP

#ifdef WIN32
	#include <wincrypt.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "autogen_ConnectionException.h"
#include "autogen_ServerException.h"
#include "CryptoUtils.h"
#include "Exception.h"
#include "SSLLib.h"

#include "MemLeakFindOn.h"

#ifndef BOX_RELEASE_BUILD
	bool SSLLib__TraceErrors = false;
#endif

// --------------------------------------------------------------------------
//
// Function
//		Name:    SSLLib::Initialise()
//		Purpose: Initialise SSL library
//		Created: 2003/08/06
//
// --------------------------------------------------------------------------
void SSLLib::Initialise()
{
	if(!::SSL_library_init())
	{
		THROW_EXCEPTION_MESSAGE(ServerException,
			SSLLibraryInitialisationError,
			CryptoUtils::LogError("initialising OpenSSL"));
	}
	
	// More helpful error messages
	::SSL_load_error_strings();

	// Extra seeding over and above what's already done by the library
#ifdef WIN32
	HCRYPTPROV provider;
	if(!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL,
		CRYPT_VERIFYCONTEXT))
	{
		BOX_LOG_WIN_ERROR("Failed to acquire crypto context");
		BOX_WARNING("No random device -- additional seeding of "
			"random number generator not performed.");
	}
	else
	{
		// must free provider
		BYTE buf[1024];

		if(!CryptGenRandom(provider, sizeof(buf), buf))
		{
			BOX_LOG_WIN_ERROR("Failed to get random data");
			BOX_WARNING("No random device -- additional seeding of "
				"random number generator not performed.");
		}
		else
		{
			RAND_seed(buf, sizeof(buf));
		}
		
		if(!CryptReleaseContext(provider, 0))
		{
			BOX_LOG_WIN_ERROR("Failed to release crypto context");
		}
	}
#elif defined HAVE_RANDOM_DEVICE
	if(::RAND_load_file(RANDOM_DEVICE, 1024) != 1024)
	{
		THROW_EXCEPTION(ServerException, SSLRandomInitFailed)
	}
#else
	BOX_WARNING("No random device -- additional seeding of "
		"random number generator not performed.");
#endif
}


