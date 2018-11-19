#ifndef WIN32_INTRNL_H
#define WIN32_INTRNL_H
#if defined(__MINGW32__) && !defined(_WIN64)
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#include <tdiinfo.h>

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned int uint;

/* forward declare, each module has their own idea of what this is */
typedef struct _InterfaceIndexTable InterfaceIndexTable;

typedef struct IFEntry
{
  ulong if_index;
  ulong if_type;
  ulong if_mtu;
  ulong if_speed;
  ulong if_physaddrlen;
  uchar if_physaddr[8];
  ulong if_adminstatus;
  ulong if_operstatus;
  ulong if_lastchange;
  ulong if_inoctets;
  ulong if_inucastpkts;
  ulong if_innucastpkts;
  ulong if_indiscards;
  ulong if_inerrors;
  ulong if_inunknownprotos;
  ulong if_outoctets;
  ulong if_outucastpkts;
  ulong if_outnucastpkts;
  ulong if_outdiscards;
  ulong if_outerrors;
  ulong if_outqlen;
  ulong if_descrlen;
  uchar if_descr[1];
} IFEntry;

typedef struct IPAddrEntry
{
  ulong iae_addr;
  ulong iae_index;
  ulong iae_mask;
  ulong iae_bcastaddr;
  ulong iae_reasmsize;
  ushort iae_context;
  ushort iae_pad;
} IPAddrEntry;

typedef union _IFEntrySafelySized {
  CHAR MaxSize[sizeof(DWORD) + sizeof(IFEntry) + 128 + 1];
  IFEntry ent;
} IFEntrySafelySized;

#ifndef IFENT_SOFTWARE_LOOPBACK
#define IFENT_SOFTWARE_LOOPBACK 24 /* This is an SNMP constant from rfc1213 */
#endif                             /*IFENT_SOFTWARE_LOOPBACK*/

/* Encapsulates information about an interface */
typedef struct _IFInfo
{
  TDIEntityID entity_id;
  IFEntrySafelySized if_info;
  IPAddrEntry ip_addr;
} IFInfo;

/* functions */
NTSTATUS
openTcpFile(PHANDLE tcpFile, ACCESS_MASK DesiredAccess);

VOID
closeTcpFile(HANDLE h);

BOOL
isLoopback(HANDLE tcpFile, TDIEntityID* loop_maybe);

BOOL
isIpEntity(HANDLE tcpFile, TDIEntityID* ent);

NTSTATUS
getNthIpEntity(HANDLE tcpFile, DWORD index, TDIEntityID* ent);

BOOL
isInterface(TDIEntityID* if_maybe);

NTSTATUS
getInterfaceInfoSet(HANDLE tcpFile, IFInfo** infoSet, PDWORD numInterfaces);

NTSTATUS
getInterfaceInfoByName(HANDLE tcpFile, char* name, IFInfo* info);

NTSTATUS
getInterfaceInfoByIndex(HANDLE tcpFile, DWORD index, IFInfo* info);

NTSTATUS
getIPAddrEntryForIf(HANDLE tcpFile, char* name, DWORD index, IFInfo* ifInfo);

InterfaceIndexTable*
getInterfaceIndexTableInt(BOOL nonLoopbackOnly);

InterfaceIndexTable*
getInterfaceIndexTable(void);
#endif

#endif