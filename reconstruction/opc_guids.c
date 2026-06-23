#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>

/*
 * Compile the OPC Foundation-generated GUID definitions from the installed
 * public headers. The interface declarations remain external dependencies;
 * no proprietary FactorySoft definitions are used here.
 */
#include "opcda_i.c"
#include "opccomn_i.c"
#include "OpcEnum_i.c"
