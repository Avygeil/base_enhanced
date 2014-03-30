// hide these nasty warnings

#ifdef _WIN32

#ifdef __ICL //intel compiler specific
#pragma warning(disable : 1)       // last line of file ends without a newline
#pragma warning(disable : 271)     // trailing comma is nonstandard
#pragma warning(disable : 344)     // typedef name has already been declared (with same type)
#pragma warning(disable : 188)     // enumerated type mixed with another type
#pragma warning(disable : 193)     // zero used for undefined preprocessing identifier
#pragma warning(disable : 306)     // nested comment is not allowed
//#pragma warning(disable : 593)     // variable was set but never used
#pragma warning(disable : 9)     
#pragma warning(disable : 111)     // statement is unreachable
#pragma warning(disable : 981)     // operands are evaluated in unspecified order
#pragma warning(disable : 1418)    // external function definition with no prior declaration
#pragma warning(disable : 1419)    // external declaration in primary source file
#pragma warning(disable : 1572)    // floating-point equality and inequality comparisons are unreliable
#pragma warning(disable : 1599)    // declaration hides variable
#pragma warning(disable : 2557)    // comparison between signed and unsigned operands
#pragma warning(disable : 2259)    // non-pointer conversion
#endif

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4032)
#pragma warning(disable : 4051)
#pragma warning(disable : 4057)		// slightly different base types
#pragma warning(disable : 4100)		// unreferenced formal parameter
#pragma warning(disable : 4115)
#pragma warning(disable : 4125)		// decimal digit terminates octal escape sequence
#pragma warning(disable : 4127)		// conditional expression is constant
#pragma warning(disable : 4136)
#pragma warning(disable : 4152)		// nonstandard extension, function/data pointer conversion in expression
#pragma warning(disable : 4201)
#pragma warning(disable : 4214)
#pragma warning(disable : 4244)		// conversion from double to float
#pragma warning(disable : 4284)		// return type not UDT
#pragma warning(disable : 4305)		// truncation from const double to float
#pragma warning(disable : 4310)		// cast truncates constant value
#pragma warning(disable : 4389)		// signed/unsigned mismatch
#pragma warning(disable : 4503)		// decorated name length truncated
//#pragma warning(disable:  4505)!!!remove these to reduce vm size!! // unreferenced local function has been removed
#pragma warning(disable : 4511)		//copy ctor could not be genned
#pragma warning(disable : 4512)		//assignment op could not be genned
#pragma warning(disable : 4514)		// unreffed inline removed
#pragma warning(disable : 4663)		// c++ lang change
#pragma warning(disable : 4702)		// unreachable code
#pragma warning(disable : 4710)		// not inlined
#pragma warning(disable : 4711)		// selected for automatic inline expansion
#pragma warning(disable : 4220)		// varargs matches remaining parameters
#pragma warning(disable : 4786)		//identifier was truncated

//rww (for vc.net, warning numbers changed apparently):
#pragma warning(disable : 4213)		//nonstandard extension used : cast on l-value
#pragma warning(disable : 4245)		//signed/unsigned mismatch

#endif
