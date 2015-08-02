#include "g_local.h"

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#include <Dbghelp.h>
#else // linux
#include <stdlib.h>
#include <execinfo.h>
#include <unistd.h>
#include <assert.h>
#include <string.h> // memset
#include <limits.h> 
#define __USE_GNU
#include <ucontext.h>
#include <signal.h>
#endif

#ifdef _WIN32
char* GetExceptionName(DWORD code)
{
	switch(code)
	{
	case EXCEPTION_ACCESS_VIOLATION:		 return "Access Violation";
	case EXCEPTION_DATATYPE_MISALIGNMENT:    return "Datatype Misalignment";
	case EXCEPTION_BREAKPOINT:               return "Breakpoint";
	case EXCEPTION_SINGLE_STEP:              return "Single Step";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "Array Bounds Exceeded";
	case EXCEPTION_FLT_DENORMAL_OPERAND:     return "Float Denormal Operand";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "Float Divide By Zero";
	case EXCEPTION_FLT_INEXACT_RESULT:       return "Float Inexact Result";
	case EXCEPTION_FLT_INVALID_OPERATION:    return "Float Invalid Operation";
	case EXCEPTION_FLT_OVERFLOW:             return "Float Overflow";
	case EXCEPTION_FLT_STACK_CHECK:          return "Float Stack Check";
	case EXCEPTION_FLT_UNDERFLOW:            return "Float Underflow";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "Integer Divide By Zero";
	case EXCEPTION_INT_OVERFLOW:             return "Integer Overflow";
	case EXCEPTION_PRIV_INSTRUCTION:         return "Private Instruction";
	case EXCEPTION_IN_PAGE_ERROR:            return "In Page Error";
	case EXCEPTION_ILLEGAL_INSTRUCTION:      return "Illegal Instruction";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Non-continuable Exceptiopn";
	case EXCEPTION_STACK_OVERFLOW:           return "Stack Overflow";
	case EXCEPTION_INVALID_DISPOSITION:      return "Invalid Disposition";
	case EXCEPTION_GUARD_PAGE:               return "Guard Page";
	case EXCEPTION_INVALID_HANDLE:           return "Invalid Handle";
	case 0xE06D7363:  return "C++ Exception (using throw)";

	default:
		return "Unknown Exception";
	}

}

void Win_PrintCallStack(FILE *output, HANDLE thread, CONTEXT* pContext)
{
    BOOL            result;
    HANDLE          process;
	CONTEXT			context;
    STACKFRAME64        stack;
    ULONG               frame;
    IMAGEHLP_SYMBOL64*		symbol;
    DWORD64             displacement;
	DWORD64			moduleAddress;
	DWORD baseOffset;
	char symbolBuffer[sizeof(IMAGEHLP_SYMBOL64)+256];
	char moduleName[128];

	// redirect to standard output by default
	if (!output)
		output = stdout;

	// trace stack for this thread by default
	if (!thread)
		thread = GetCurrentThread();

	// use current context by default
	if (!pContext)
	{
		memset( &context, 0, sizeof( context ) );
		RtlCaptureContext( &context );
		pContext = &context;
	}

	// stack info initialization
	memset( &symbolBuffer, 0, sizeof( symbolBuffer ) );
	symbol = (IMAGEHLP_SYMBOL64*)symbolBuffer;
    memset( &stack, 0, sizeof( STACKFRAME64 ) );
    process                = GetCurrentProcess();
    displacement           = 0;
    stack.AddrPC.Offset    = pContext->Eip;
    stack.AddrPC.Mode      = AddrModeFlat;
    stack.AddrStack.Offset = pContext->Esp;
    stack.AddrStack.Mode   = AddrModeFlat;
    stack.AddrFrame.Offset = pContext->Ebp;
    stack.AddrFrame.Mode   = AddrModeFlat;

	// symbols inicialization (using Dbghelp.dll)
	SymInitialize( process, NULL, TRUE );

	frame = 0;
	baseOffset = (DWORD)GetModuleHandle(NULL);

	fprintf(output, "\n-----------------------------------\n");
	fprintf(output, "          Call Stack Trace           \n");
	fprintf(output, "-----------------------------------\n");
    while( result = StackWalk64( IMAGE_FILE_MACHINE_I386, process, thread, &stack,
            NULL, NULL, NULL, SymGetModuleBase64, NULL ) )
	{		
		// get module name
		moduleAddress = SymGetModuleBase64(process, stack.AddrPC.Offset);
		GetModuleBaseName(process, (HMODULE) moduleAddress,	moduleName, sizeof(moduleName));

		// attempt to load symbol information (name/offset)
        symbol->SizeOfStruct  = sizeof( IMAGEHLP_SYMBOL64 );
        symbol->MaxNameLength = 256;
		SymGetSymFromAddr64( process, ( ULONG64 )stack.AddrPC.Offset, &displacement, symbol );

		// with symbols
		if ( symbol->Address && (stack.AddrPC.Offset >= symbol->Address) )
		{
			fprintf( output, "%s(%s+0x%llX) [0x%llX]\n", 
					moduleName, symbol->Name, stack.AddrPC.Offset-symbol->Address, stack.AddrPC.Offset );
		} 
		else 
		{
			fprintf( output, "%s [0x%llX]\n", 
				moduleName, stack.AddrPC.Offset );
		}

		++frame;
	}
}
#else // linux
#define MAX_CALLSTACK_BUFFER 100

#define CRASH_MAX_BACKTRACE_DEPTH (25)

/* Get a backtrace from a signal handler.
* array is place to put array
* size is it's size
* context is a pointer to the mysterious signal ahndler 3rd parameter with the registers
* distance is the distance is calls from the signal handler
*
*/
inline unsigned int signal_backtrace(void ** array, unsigned int size, ucontext_t * context, unsigned int distance) {

	/* WARNING: If you ever remove the inline from the function prototype,
	* adjust this to match!!!
	*/
	#define IP_STACK_FRAME_NUMBER (3)

	unsigned int ret = backtrace(array, size);
	distance += IP_STACK_FRAME_NUMBER;

	assert(distance <= size);

	/* OK, here is the tricky part:
	*
	* Linux signal handling on some archs works by the kernel replacing, in situ, the
	* return address of the faulting function on the faulting thread user space stack with
	* that of the Glibc signal unwind handling routine and coercing user space to just to
	* glibc signal handler preamble. Later the signal unwind handling routine undo this.
	*
	* What this means for us is that the backtrace we get is missing the single most important
	* bit of information: the addres of the faulting function.
	*
	* We get it back using the undocumented 3rs parameter to the signal handler call back
	* with used in it's SA_SIGINFO form which contains access to the registers kept during
	* the fault. We grab the IP from there and 'fix' the backtrace.
	*
	* This needs to be different per arch, of course.
	*/

	#ifdef __i386__
	array[distance] = (void *)(context->uc_mcontext.gregs[REG_EIP]);
	#endif /* __i386__ */

	#ifdef __PPC__
	array[distance] = (void *)(context->uc_mcontext.regs->nip);
	#endif /* __PPC__ */

	return ret;
}

void Linux_PrintCallStack(FILE* output, ucontext_t* context, int maxframes){
	int i;
	void* buffer[MAX_CALLSTACK_BUFFER];
	char** strings;
	size_t num_backtrace_frames;

	if (maxframes > MAX_CALLSTACK_BUFFER){
		maxframes = MAX_CALLSTACK_BUFFER;
	}

	fprintf(output, "\n-----------------------------------\n");
	fprintf(output, "          Call Stack Trace           \n");
	fprintf(output, "-----------------------------------\n");

	num_backtrace_frames = signal_backtrace(buffer,
		maxframes, context, 0);
	strings = backtrace_symbols(buffer, num_backtrace_frames);
    if (strings == NULL) {
		fprintf(output, "Failed to retrieve callstack.\n");
        return;
    }

    for (i = 0; i < num_backtrace_frames; i++){
		fprintf(output, "%s\n", strings[i]);
	}

    free(strings);
}

#endif

void PrintJKAInfo(FILE* file)
{
	//JKA Info variables
	int i, clientcount;
	static char mapname[128];

	trap_Cvar_VariableStringBuffer("mapname",mapname,sizeof(mapname));
	fprintf(file, "\n-----------------------------------\n");
	fprintf(file, "           JKA Information           \n");
	fprintf(file, "-----------------------------------\n");
	fprintf(file, "Map: %s\n", mapname ); 
	fprintf(file, "Gametype: %i\n", g_gametype.integer ); 
	fprintf(file, "Server Time: %i ms\n", level.time ); 
	fprintf(file, "Map Time: %i ms\n", level.time - level.startTime ); 
	fprintf(file, "Scores: %i %i (Red,Blue)\n", 
		level.teamScores[TEAM_RED], 
		level.teamScores[TEAM_BLUE] ); 

	for(i=0,clientcount=0;i<level.maxclients;++i){
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			++clientcount;
		}
	}

	fprintf(file, "\nPlayers Count: %i/%i\n", clientcount, level.maxclients );
	fprintf(file, "%-7s %4s %-32s %4s %22s %-4s\n", 
			"","Slot", "Name", "Ping", "IP", "Team");

	for(i=0;i<level.maxclients;++i)
	{
		char* teamname = "";
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			switch(level.clients[i].sess.sessionTeam){
				case TEAM_RED:			teamname = "RED";    break;				
				case TEAM_BLUE:			teamname = "BLUE";   break;
				case TEAM_FREE:			teamname = "FREE";   break;
				case TEAM_SPECTATOR:	teamname = "SPEC";   break;
				default: break;
			};

			fprintf(file, "%-7s %-4i %-32s %4i %22s %-4s\n", 
				"",
				level.clients[i].pers.clientNum,
				level.clients[i].pers.netname, 
				level.clients[i].ps.ping,
				level.clients[i].sess.ipString,
				teamname
				);
		}
	}

}

#ifdef _WIN32
LONG Win_Handler(LPEXCEPTION_POINTERS p)
{
	HANDLE process, thread;
	static char ProcessName[128],ModuleName[128];
	static char CrashLogFileName[128];
	time_t rawtime;
	struct tm * timeinfo;
	FILE* CrashLogFile;
	DWORD64 moduleAddress;
		
	process = GetCurrentProcess();
	thread = GetCurrentThread();
	
	// symbols inicialization (using Dbghelp.dll)
	SymInitialize( process, NULL, TRUE );

	// get process name
	GetModuleFileName(NULL,ProcessName,sizeof(ProcessName));

	// get module name
	moduleAddress = SymGetModuleBase64(process, (DWORD64)p->ExceptionRecord->ExceptionAddress);
	GetModuleBaseName(GetCurrentProcess(), (HMODULE) moduleAddress,	ModuleName, sizeof(ModuleName));

	// generate crash log file name
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	strftime(CrashLogFileName, sizeof(CrashLogFileName), "crash_log_%Y-%m-%d_%H-%M-%S.log", timeinfo);

	// open crash log file
	CrashLogFile = fopen(CrashLogFileName,"w");
	
	fprintf(CrashLogFile, "===================================\n");
	fprintf(CrashLogFile, "    JKA base_enhanced Crash Log       \n");
	fprintf(CrashLogFile, "===================================\n");
	fprintf(CrashLogFile, "Process File Name: %s\n", ProcessName	);
	fprintf(CrashLogFile, "Module File Name: %s\n", ModuleName	);
	fprintf(CrashLogFile, "Exception Code: 0x%08X (%s)\n", 
		p->ExceptionRecord->ExceptionCode, GetExceptionName(p->ExceptionRecord->ExceptionCode));  
	fprintf(CrashLogFile, "Exception Address: 0x%08X (%s+0x%X)\n", 
		p->ExceptionRecord->ExceptionAddress,ModuleName,(DWORD)p->ExceptionRecord->ExceptionAddress - moduleAddress);  

	fprintf(CrashLogFile, "\n-----------------------------------\n");
	fprintf(CrashLogFile, "           Register Dump           \n");
	fprintf(CrashLogFile, "-----------------------------------\n");
	fprintf(CrashLogFile, "General Purpose & Control Registers:\n");
	fprintf(CrashLogFile, "EAX: 0x%08X, EBX: 0x%08X, ECX: 0x%08X\n",
		p->ContextRecord->Eax, p->ContextRecord->Ebx, p->ContextRecord->Ecx ); 
	fprintf(CrashLogFile, "EDX: 0x%08X, EBP: 0x%08X, EDI: 0x%08X\n",
		p->ContextRecord->Edx, p->ContextRecord->Ebp, p->ContextRecord->Edi ); 
	fprintf(CrashLogFile, "EIP: 0x%08X, ESI: 0x%08X, ESP: 0x%08X\n", 
		p->ContextRecord->Eip, p->ContextRecord->Esi, p->ContextRecord->Esp ); 
	fprintf(CrashLogFile, "\nSegment Registers:\n");
	fprintf(CrashLogFile, "CS: 0x%08X, DS: 0x%08X, ES: 0x%08X\n", 
		p->ContextRecord->SegCs, p->ContextRecord->SegDs, p->ContextRecord->SegEs ); 
	fprintf(CrashLogFile, "FS: 0x%08X, GS: 0x%08X, SS: 0x%08X\n", 
		p->ContextRecord->SegFs, p->ContextRecord->SegGs, p->ContextRecord->SegSs ); 

	Win_PrintCallStack(CrashLogFile, thread, p->ContextRecord);	

	// print JKA Info
	PrintJKAInfo(CrashLogFile);

	// close crash log file
	fclose(CrashLogFile);

	return EXCEPTION_EXECUTE_HANDLER;
}

#else //linux
/* This translates a signal code into a readable string */
static inline char * code2str(int code, int signal) 
{
	switch(code) {
	case SI_USER:
	return "kill, sigsend or raise ";
	case SI_KERNEL:
	return "kernel";
	case SI_QUEUE:
	return "sigqueue";
	}

	if(SIGILL==signal) switch(code) {
	case ILL_ILLOPC:
	return "illegal opcode";
	case ILL_ILLOPN:
	return "illegal operand";
	case ILL_ILLADR:
	return "illegal addressing mode";
	case ILL_ILLTRP:
	return "illegal trap";
	case ILL_PRVOPC:
	return "privileged register";
	case ILL_COPROC:
	return "coprocessor error";
	case ILL_BADSTK:
	return "internal stack error";
	}

	if(SIGFPE==signal) switch(code) {
	case FPE_INTDIV:
	return "integer divide by zero";
	case FPE_INTOVF:
	return "integer overflow";
	case FPE_FLTDIV:
	return "floating point divide by zero";
	case FPE_FLTOVF:
	return "floating point overflow";
	case FPE_FLTUND:
	return "floating point underflow";
	case FPE_FLTRES:
	return "floating point inexact result";
	case FPE_FLTINV:
	return "floating point invalid operation";
	case FPE_FLTSUB:
	return "subscript out of range";
	}

	if(SIGSEGV==signal) switch(code) {
	case SEGV_MAPERR:
	return "address not mapped to object";
	case SEGV_ACCERR:
	return "invalid permissions for mapped object";
	}

	if(SIGBUS==signal) switch(code) {
	case BUS_ADRALN:
	return "invalid address alignment";
	case BUS_ADRERR:
	return "non-existent physical address";
	case BUS_OBJERR:
	return "object specific hardware error";
	}

	if(SIGTRAP==signal) switch(code) {
	case TRAP_BRKPT:
	return "process breakpoint";
	case TRAP_TRACE:
	return "process trace trap";
	}

	return "Unhandled signal handler";
}

extern const char *__progname;
void Linux_Handler (int signo, siginfo_t * siginfo, void *context)
{
	char CrashLogFileName[128];
	time_t rawtime;
	struct tm * timeinfo;
	FILE* CrashLogFile;
	ucontext_t *ucontext = (ucontext_t *)context;

	// generate crash log file name
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	strftime(CrashLogFileName, sizeof(CrashLogFileName), "crash_log_%Y-%m-%d_%H-%M-%S.log", timeinfo);

	// open crash log file
	CrashLogFile = fopen(CrashLogFileName,"w");

	fprintf(CrashLogFile, "===================================\n");
	fprintf(CrashLogFile, "    JKA base_enhanced Crash Log       \n");
	fprintf(CrashLogFile, "===================================\n");
	fprintf(CrashLogFile, "Process File Name: %s\n", __progname	);
	fprintf(CrashLogFile, "Process ID: %d\n", getpid()	);
	fprintf(CrashLogFile, "Signal Code: 0x%08X \n", signo);
	fprintf(CrashLogFile, "Signal Reason: %s\n", code2str(siginfo->si_code, signo));  
	
	fprintf(CrashLogFile, "Exception Address: 0x%08X \n", (unsigned int)(siginfo->si_addr));  

	fprintf(CrashLogFile, "\n-----------------------------------\n");
	fprintf(CrashLogFile, "           Register Dump           \n");
	fprintf(CrashLogFile, "-----------------------------------\n");
	fprintf(CrashLogFile, "General Purpose & Control Registers:\n");
	fprintf(CrashLogFile, "EAX: 0x%08X, EBX: 0x%08X, ECX: 0x%08X\n",
		ucontext->uc_mcontext.gregs[REG_EAX], 
		ucontext->uc_mcontext.gregs[REG_EBX], 
		ucontext->uc_mcontext.gregs[REG_ECX] ); 
	fprintf(CrashLogFile, "EDX: 0x%08X, EBP: 0x%08X, EDI: 0x%08X\n",
		ucontext->uc_mcontext.gregs[REG_EDX], 
		ucontext->uc_mcontext.gregs[REG_EBP], 
		ucontext->uc_mcontext.gregs[REG_EDI] ); 
	fprintf(CrashLogFile, "EIP: 0x%08X, ESI: 0x%08X, ESP: 0x%08X\n", 
		ucontext->uc_mcontext.gregs[REG_EIP], 
		ucontext->uc_mcontext.gregs[REG_ESI], 
		ucontext->uc_mcontext.gregs[REG_ESP] ); 

	Linux_PrintCallStack(CrashLogFile, context, 100);

	// print JKA Info
	PrintJKAInfo(CrashLogFile);
	
	// close crash log file
	fclose(CrashLogFile);

	abort();
}
#endif

void InitUnhandledExceptionFilter()
{
#ifdef _WIN32
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)&Win_Handler);
#else
	// SIGSEGV, SIGTERM, SIGKILL 
	struct sigaction act;
	memset(&act, 0, sizeof (act));
	act.sa_sigaction = Linux_Handler;
	sigfillset (&act.sa_mask);

	act.sa_flags = SA_SIGINFO;

	sigaction (SIGSEGV, &act, NULL); 
	sigaction (SIGFPE, &act, NULL);
	sigaction (SIGQUIT, &act, NULL);
	sigaction (SIGTERM, &act, NULL);
	sigaction (SIGKILL, &act, NULL);
#endif
}


