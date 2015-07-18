#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

void G_DbLoad();
void G_DbUnload();

void G_DbAddToWhitelist( const char* ip, 
    const char* mask );

void G_DbAddToBlacklist( const char* ip, 
    const char* mask, 
    const char* notes );

void G_DbRemoveFromBlacklist( const char* ip,
    const char* mask );

void G_DbRemoveFromWhitelist( const char* ip,
    const char* mask );

qboolean G_DbIsWhiteListed( int ipA, int ipB, int ipC, int ipD );
qboolean G_DbIsBlackListed( int ipA, int ipB, int ipC, int ipD );

void G_DbLogLevel();
qboolean G_DbIsIpForbidden( const char* ip );


#endif //G_DATABASE_H


