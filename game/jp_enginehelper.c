// jp_enginehelper.c

#include "jp_engine.h"

//#if 0

// ==================================================
// UnlockMemory (WIN32 & Linux compatible)
// --------------------------------------------------
// Makes the memory at address writable for at least size bytes.
// Returns 1 if successful, returns 0 on failure.
// ==================================================
bool UnlockMemory( int address, int size )
{
#ifdef _WIN32
	DWORD dummy;
	return ( VirtualProtect( (LPVOID)address, size, PAGE_EXECUTE_READWRITE, &dummy ) != 0 );
#else
	// Linux is a bit more tricky
	int ret;
	int page1, page2;
	page1 = address & ~( getpagesize() - 1);
	page2 = (address+size) & ~( getpagesize() - 1);

	if ( page1 == page2 )
		return (mprotect( (char *)page1, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC ) == 0);
	else
	{
		ret = mprotect( (char *)page1, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC );
		if ( ret ) return 0;
		return (mprotect( (char *)page2, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC ) == 0);
	}
#endif
}

// ==================================================
// LockMemory (WIN32 & Linux compatible)
// --------------------------------------------------
// Makes the memory at address read-only for at least size bytes.
// Returns 1 if successful, returns 0 on failure.
// ==================================================
bool LockMemory( int address, int size )
{
#ifdef _WIN32
	DWORD dummy;
	return ( VirtualProtect( (LPVOID)address, size, PAGE_EXECUTE_READ, &dummy ) != 0 );
#else
	// Linux is a bit more tricky
	int ret;
	int page1, page2;
	page1 = address & ~( getpagesize() - 1);
	page2 = (address+size) & ~( getpagesize() - 1);

	if( page1 == page2 )
		return (mprotect( (char *)page1, getpagesize(), PROT_READ | PROT_EXEC ) == 0);
	else
	{
		ret = mprotect( (char *)page1, getpagesize(), PROT_READ | PROT_EXEC );
		if ( ret ) return 0;
		return (mprotect( (char *)page2, getpagesize(), PROT_READ | PROT_EXEC ) == 0);
	}
#endif
}

#ifdef PATCH_ENGINE
bool ReplaceMemory(replaceEntry_t *replace){
	bool success = false;

	if (replace && (success = UnlockMemory( replace->replacePosition, replace->size ))){
		memcpy( &replace->origBytes[0], (void *)replace->replacePosition, replace->size );

		//replace
		memcpy( (void *)replace->replacePosition, replace->replBytes, replace->size );
		success = LockMemory( replace->replacePosition, replace->size );
	}

	if (!success){
		Com_Printf( "^1Warning: Failed to replace memory: %s\n", replace->name);
	}
	return success;
//#ifdef _DEBUG
//	Com_Printf( success ? va( "  %s\n", replace->name )
//						: va( "^1Warning: Failed to replace memory: %s\n", replace->name ) );
//#endif
}

bool RestoreMemory(const replaceEntry_t *replace){
	bool success = false;

	if (replace && (success = UnlockMemory( replace->replacePosition, replace->size ))){
		//replace
		memcpy( (void *)replace->replacePosition, replace->origBytes, replace->size );
		success = LockMemory( replace->replacePosition, replace->size );
	}

	if (!success){
		Com_Printf( "^1Warning: Failed to restore memory: %s\n", replace->name);
	}
	return success;
//#ifdef _DEBUG
//	Com_Printf( success ? va( "  %s\n", replace->name )
//						: va( "^1Warning: Failed to restore memory: %s\n", replace->name ) );
//#endif
}


bool PlaceHook( hookEntry_t *hook )
{
	bool success = false;

	if ( hook && (success = UnlockMemory( hook->hookPosition, 5 )) )
	{
		unsigned int forward = (unsigned int)((void*(*)())hook->hookForward)(); //i never want to see this line again
		memcpy( &hook->origBytes[0], (void *)hook->hookPosition, 5 );
		*(unsigned char *)(hook->hookPosition) = hook->hookOpcode;
		*(unsigned int *)(hook->hookPosition+1) = (forward) - ((unsigned int)(hook->hookPosition)+5);
		success = LockMemory( hook->hookPosition, 5 );
	}

	if (!success){
		Com_Printf( "^1Warning: Failed to place hook: %s\n", hook->name);
	}
	return success;
//#ifdef _DEBUG
//	Com_Printf( success ? va( "  %s\n", hook->name )
//						: va( "^1Warning: Failed to place hook: %s\n", hook->name ) );
//#endif
}

bool RemoveHook( const hookEntry_t *hook )
{
	bool success = false;

	if ( hook && (success = UnlockMemory( hook->hookPosition, 5 )) )
	{
		memcpy( (void *)hook->hookPosition, hook->origBytes, 5 );
		success = LockMemory( hook->hookPosition, 5 );
	}

	if (!success){
		Com_Printf( "^1Warning: Failed to remove hook: %s\n", hook->name);
	}
	return success;
//#ifdef _DEBUG
//	Com_Printf( success ? va( "  %s\n", hook->name )
//						: va( "^1Warning: Failed to remove hook: %s\n", hook->name ) );
//#endif
}


#endif

//#endif
