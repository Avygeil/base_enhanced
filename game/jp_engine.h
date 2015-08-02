// jp_engine.h
// 
//#if 0

#if defined(_WIN32) && !defined(MINGW32)
#include <windows.h> 
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

#include "q_shared.h"
//#include "ui_shared.h"

#define PATCH_ENGINE
#define HOOK_GETSTATUS_FIX
#define HOOK_INFOBOOM_FIX
#define HOOK_NEGATIVEPORT_FIX
#define FULLSERVER_CONNECT_REPORT
#define HOOK_RCONExtensions
#define HOOK_DoneDL
#define HOOK_TIMEWARPPING_FIX

typedef struct hookEntry_s
{
	const unsigned int	hookPosition;	//	The code we're patching
	unsigned char		origBytes[5];	//	What it originally was
	unsigned char		hookOpcode;		//	CALL or JMP
	const unsigned int	hookForward;	//	Function to direct the control flow into
	const char			*name;			//	Long name of the hook
} hookEntry_t;

typedef struct replaceEntry_s
{
	const unsigned int	replacePosition;	//	The code we're patching
	unsigned char		origBytes[16];	//	What it originally was
	unsigned char		replBytes[16];	//	What it originally was
	unsigned int		size;
	const char			*name;			//	Long name of the replace
} replaceEntry_t;

typedef unsigned int bool;
#define false 0

#if defined(__GCC__) || defined(MINGW32)
	#define NORET __attribute__((noreturn))
	#define USED __attribute__((used))
#else //defined(__GCC__) || defined(MINGW32)
	#define NORET
	#define USED
#endif //defined(__GCC__) || defined(MINGW32)

// Cross-platform (Linux & Windows) Asm defines

#if defined(_WIN32) && !defined(MINGW32)

	#define __asm1__( a )		__asm a
	#define __asm2__( a, b )	__asm a, b
	#define __asmL__( a )		__asm a

	#define __StartHook( a )	__asm lea eax, [__hookstart] \
								__asm jmp __hookend \
								__asm __hookstart:
	#define __EndHook( a )		__asm __hookend:

#else

// Linux has a few issues with defines in function defines
// So we'll create a double define to ensure the defines are properly processed
	#define __asm1__i( a )		__asm__( #a "\n" );
	#define __asm2__i( a, b )	__asm__( #a ", " #b "\n" );
        // proxy defines
        #define __asm1__( a )           __asm1__i( a )
        #define __asm2__( a, b )        __asm2__i( a, b )

	#define __asmL__( a )      	__asm__( #a "\n" );

	#define __StartHook( a )	__asm__( ".intel_syntax noprefix\n" ); \
								__asm__("lea eax, [__hookstart" #a "]\n"); \
								__asm__("jmp __hookend" #a "\n"); \
								__asm__(".att_syntax\n"); \
								__asm__("__hookstart" #a ":\n"); \
								__asm__(".intel_syntax noprefix\n");

	#define __EndHook( a )		__asm__(".att_syntax\n"); \
								__asm__("__hookend" #a ":\n"); \
								__asm__(".intel_syntax noprefix\n");

#endif

#define HOOKDEF( pos, opcode, fwd, name ) { pos, {0x00}, opcode, (unsigned int)fwd, name }
#define HOOK( name ) void NORET *Hook_##name( void )

#define SEP ,
#define REPLACEDEF( pos, what, size, name ) { pos, {0x00}, what, size, name }


#define TABLESIZE( table, count) static int count = sizeof(table)/sizeof(table[0])

//Helper functions
bool UnlockMemory( int address, int size );
bool LockMemory( int address, int size );
#ifdef PATCH_ENGINE
	bool PlaceHook( hookEntry_t *hook );
	bool RemoveHook( const hookEntry_t *hook );
	bool ReplaceMemory(replaceEntry_t *replace);
	bool RestoreMemory(const replaceEntry_t *replace);
#endif

//API
qboolean PatchEngine( void );
qboolean UnpatchEngine( void );
extern qboolean enginePatched;

//Hooks declarations
#ifdef HOOK_GETSTATUS_FIX
extern unsigned int testip;
HOOK( GetStatusFix );
HOOK( GetInfoFix );
#endif
#ifdef FULLSERVER_CONNECT_REPORT
HOOK( FullServerConnect );
#endif

//#endif

