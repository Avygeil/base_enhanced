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

#pragma warning(disable : 4244)		// conversion from double to float
#pragma warning(disable : 4305)		// truncation from const double to float
  
#endif
