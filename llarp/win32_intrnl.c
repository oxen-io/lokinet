#if defined(__MINGW32__) && !defined(_WIN64)
/*
 * All the user-mode scaffolding necessary to backport GetAdaptersAddresses(2))
 * to the NT 5.x series. See further comments for any limitations.
 *
 * -despair86 30/07/18
 */
#include <assert.h>
#include <stdio.h>

// apparently mingw-w64 loses its shit over this
// but only for 32-bit builds, naturally
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

// these need to be in a specific order
#include <windows.h>
#include <winternl.h>
#include <tdi.h>
#include "win32_intrnl.h"

const PWCHAR TcpFileName = L"\\Device\\Tcp";

// from ntdll.dll
typedef void(FAR PASCAL *pRtlInitUString)(UNICODE_STRING *, const WCHAR *);
typedef NTSTATUS(FAR PASCAL *pNTOpenFile)(HANDLE *, ACCESS_MASK,
                                          OBJECT_ATTRIBUTES *,
                                          IO_STATUS_BLOCK *, ULONG, ULONG);
typedef NTSTATUS(FAR PASCAL *pNTClose)(HANDLE);

#define FSCTL_TCP_BASE FILE_DEVICE_NETWORK

#define _TCP_CTL_CODE(Function, Method, Access) \
  CTL_CODE(FSCTL_TCP_BASE, Function, Method, Access)

#define IOCTL_TCP_QUERY_INFORMATION_EX \
  _TCP_CTL_CODE(0, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef struct _InterfaceIndexTable
{
  DWORD numIndexes;
  DWORD numAllocated;
  DWORD indexes[1];
} InterfaceIndexTable;

NTSTATUS
tdiGetMibForIfEntity(HANDLE tcpFile, TDIEntityID *ent,
                     IFEntrySafelySized *entry)
{
  TCP_REQUEST_QUERY_INFORMATION_EX req = {{{0}}};
  NTSTATUS status                      = 0;
  DWORD returnSize;

#ifdef DEBUG
  fprintf(stderr, "TdiGetMibForIfEntity(tcpFile %x,entityId %x)\n",
          (int)tcpFile, (int)ent->tei_instance);
#endif

  req.ID.toi_class  = INFO_CLASS_PROTOCOL;
  req.ID.toi_type   = INFO_TYPE_PROVIDER;
  req.ID.toi_id     = 1;
  req.ID.toi_entity = *ent;

  status =
      DeviceIoControl(tcpFile, IOCTL_TCP_QUERY_INFORMATION_EX, &req,
                      sizeof(req), entry, sizeof(*entry), &returnSize, NULL);

  if(!status)
  {
    perror("IOCTL Failed\n");
    return 0xc0000001;
  }

  fprintf(stderr,
          "TdiGetMibForIfEntity() => {\n"
          "  if_index ....................... %lx\n"
          "  if_type ........................ %lx\n"
          "  if_mtu ......................... %ld\n"
          "  if_speed ....................... %lx\n"
          "  if_physaddrlen ................. %ld\n",
          entry->ent.if_index, entry->ent.if_type, entry->ent.if_mtu,
          entry->ent.if_speed, entry->ent.if_physaddrlen);
  fprintf(stderr,
          "  if_physaddr .................... %02x:%02x:%02x:%02x:%02x:%02x\n"
          "  if_descr ....................... %s\n",
          entry->ent.if_physaddr[0] & 0xff, entry->ent.if_physaddr[1] & 0xff,
          entry->ent.if_physaddr[2] & 0xff, entry->ent.if_physaddr[3] & 0xff,
          entry->ent.if_physaddr[4] & 0xff, entry->ent.if_physaddr[5] & 0xff,
          entry->ent.if_descr);
  fprintf(stderr, "} status %08lx\n", status);

  return 0;
}

static NTSTATUS
tdiGetSetOfThings(HANDLE tcpFile, DWORD toiClass, DWORD toiType, DWORD toiId,
                  DWORD teiEntity, DWORD teiInstance, DWORD fixedPart,
                  DWORD entrySize, PVOID *tdiEntitySet, PDWORD numEntries)
{
  TCP_REQUEST_QUERY_INFORMATION_EX req = {{{0}}};
  PVOID entitySet                      = 0;
  NTSTATUS status                      = 0;
  DWORD allocationSizeForEntityArray   = entrySize * MAX_TDI_ENTITIES,
        arraySize                      = entrySize * MAX_TDI_ENTITIES;

  req.ID.toi_class               = toiClass;
  req.ID.toi_type                = toiType;
  req.ID.toi_id                  = toiId;
  req.ID.toi_entity.tei_entity   = teiEntity;
  req.ID.toi_entity.tei_instance = teiInstance;

  /* There's a subtle problem here...
   * If an interface is added at this exact instant, (as if by a PCMCIA
   * card insertion), the array will still not have enough entries after
   * have allocated it after the first DeviceIoControl call.
   *
   * We'll get around this by repeating until the number of interfaces
   * stabilizes.
   */
  do
  {
    status =
        DeviceIoControl(tcpFile, IOCTL_TCP_QUERY_INFORMATION_EX, &req,
                        sizeof(req), 0, 0, &allocationSizeForEntityArray, NULL);

    if(!status)
      return 0xc0000001;

    arraySize = allocationSizeForEntityArray;
    entitySet = HeapAlloc(GetProcessHeap(), 0, arraySize);

    if(!entitySet)
    {
      status = ((NTSTATUS)0xC000009A);
      return status;
    }

    status = DeviceIoControl(tcpFile, IOCTL_TCP_QUERY_INFORMATION_EX, &req,
                             sizeof(req), entitySet, arraySize,
                             &allocationSizeForEntityArray, NULL);

    /* This is why we have the loop -- we might have added an adapter */
    if(arraySize == allocationSizeForEntityArray)
      break;

    HeapFree(GetProcessHeap(), 0, entitySet);
    entitySet = 0;

    if(!status)
      return 0xc0000001;
  } while(TRUE); /* We break if the array we received was the size we
                  * expected.  Therefore, we got here because it wasn't */

  *numEntries   = (arraySize - fixedPart) / entrySize;
  *tdiEntitySet = entitySet;

  return 0;
}

static NTSTATUS
tdiGetEntityIDSet(HANDLE tcpFile, TDIEntityID **entitySet, PDWORD numEntities)
{
  NTSTATUS status =
      tdiGetSetOfThings(tcpFile, INFO_CLASS_GENERIC, INFO_TYPE_PROVIDER,
                        ENTITY_LIST_ID, GENERIC_ENTITY, 0, 0,
                        sizeof(TDIEntityID), (PVOID *)entitySet, numEntities);
  return status;
}

NTSTATUS
tdiGetIpAddrsForIpEntity(HANDLE tcpFile, TDIEntityID *ent, IPAddrEntry **addrs,
                         PDWORD numAddrs)
{
  NTSTATUS status;

  fprintf(stderr, "TdiGetIpAddrsForIpEntity(tcpFile 0x%p, entityId 0x%lx)\n",
          tcpFile, ent->tei_instance);

  status = tdiGetSetOfThings(tcpFile, INFO_CLASS_PROTOCOL, INFO_TYPE_PROVIDER,
                             0x102, CL_NL_ENTITY, ent->tei_instance, 0,
                             sizeof(IPAddrEntry), (PVOID *)addrs, numAddrs);

  return status;
}

static VOID
tdiFreeThingSet(PVOID things)
{
  HeapFree(GetProcessHeap(), 0, things);
}

NTSTATUS
openTcpFile(PHANDLE tcpFile, ACCESS_MASK DesiredAccess)
{
  UNICODE_STRING fileName;
  OBJECT_ATTRIBUTES objectAttributes;
  IO_STATUS_BLOCK ioStatusBlock;
  NTSTATUS status;
  pRtlInitUString _RtlInitUnicodeString;
  pNTOpenFile _NTOpenFile;
  HANDLE ntdll;

  ntdll = GetModuleHandle("ntdll.dll");
  _RtlInitUnicodeString =
      (pRtlInitUString)GetProcAddress(ntdll, "RtlInitUnicodeString");
  _NTOpenFile = (pNTOpenFile)GetProcAddress(ntdll, "NtOpenFile");
  _RtlInitUnicodeString(&fileName, TcpFileName);
  InitializeObjectAttributes(&objectAttributes, &fileName, OBJ_CASE_INSENSITIVE,
                             NULL, NULL);
  status = _NTOpenFile(tcpFile, DesiredAccess | SYNCHRONIZE, &objectAttributes,
                       &ioStatusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       FILE_SYNCHRONOUS_IO_NONALERT);
  /* String does not need to be freed: it points to the constant
   * string we provided */
  if(!NT_SUCCESS(status))
    *tcpFile = INVALID_HANDLE_VALUE;
  return status;
}
VOID
closeTcpFile(HANDLE h)
{
  pNTClose _NTClose;
  HANDLE ntdll = GetModuleHandle("ntdll.dll");
  _NTClose     = (pNTClose)GetProcAddress(ntdll, "NtClose");
  assert(h != INVALID_HANDLE_VALUE);
  _NTClose(h);
}

BOOL
isLoopback(HANDLE tcpFile, TDIEntityID *loop_maybe)
{
  IFEntrySafelySized entryInfo;
  NTSTATUS status;

  status = tdiGetMibForIfEntity(tcpFile, loop_maybe, &entryInfo);

  return NT_SUCCESS(status)
      && (entryInfo.ent.if_type == IFENT_SOFTWARE_LOOPBACK);
}

BOOL
isIpEntity(HANDLE tcpFile, TDIEntityID *ent)
{
  return (ent->tei_entity == CL_NL_ENTITY || ent->tei_entity == CO_NL_ENTITY);
}

NTSTATUS
getNthIpEntity(HANDLE tcpFile, DWORD index, TDIEntityID *ent)
{
  DWORD numEntities      = 0;
  DWORD numRoutes        = 0;
  TDIEntityID *entitySet = 0;
  NTSTATUS status        = tdiGetEntityIDSet(tcpFile, &entitySet, &numEntities);
  int i;

  if(!NT_SUCCESS(status))
    return status;

  for(i = 0; i < numEntities; i++)
  {
    if(isIpEntity(tcpFile, &entitySet[i]))
    {
      fprintf(stderr, "Entity %d is an IP Entity\n", i);
      if(numRoutes == index)
        break;
      else
        numRoutes++;
    }
  }

  if(numRoutes == index && i < numEntities)
  {
    fprintf(stderr, "Index %lu is entity #%d - %04lx:%08lx\n", index, i,
            entitySet[i].tei_entity, entitySet[i].tei_instance);
    memcpy(ent, &entitySet[i], sizeof(*ent));
    tdiFreeThingSet(entitySet);
    return 0;
  }
  else
  {
    tdiFreeThingSet(entitySet);
    return 0xc000001;
  }
}

BOOL
isInterface(TDIEntityID *if_maybe)
{
  return if_maybe->tei_entity == IF_ENTITY;
}

NTSTATUS
getInterfaceInfoSet(HANDLE tcpFile, IFInfo **infoSet, PDWORD numInterfaces)
{
  DWORD numEntities;
  TDIEntityID *entIDSet = NULL;
  NTSTATUS status       = tdiGetEntityIDSet(tcpFile, &entIDSet, &numEntities);
  IFInfo *infoSetInt    = 0;
  int curInterf         = 0, i;

  if(!NT_SUCCESS(status))
  {
    fprintf(stderr, "getInterfaceInfoSet: tdiGetEntityIDSet() failed: 0x%lx\n",
            status);
    return status;
  }

  infoSetInt = HeapAlloc(GetProcessHeap(), 0, sizeof(IFInfo) * numEntities);

  if(infoSetInt)
  {
    for(i = 0; i < numEntities; i++)
    {
      if(isInterface(&entIDSet[i]))
      {
        infoSetInt[curInterf].entity_id = entIDSet[i];
        status = tdiGetMibForIfEntity(tcpFile, &entIDSet[i],
                                      &infoSetInt[curInterf].if_info);
        fprintf(stderr, "tdiGetMibForIfEntity: %08lx\n", status);
        if(NT_SUCCESS(status))
        {
          DWORD numAddrs;
          IPAddrEntry *addrs;
          TDIEntityID ip_ent;
          int j;

          status = getNthIpEntity(tcpFile, curInterf, &ip_ent);
          if(NT_SUCCESS(status))
            status =
                tdiGetIpAddrsForIpEntity(tcpFile, &ip_ent, &addrs, &numAddrs);
          for(j = 0; NT_SUCCESS(status) && j < numAddrs; j++)
          {
            fprintf(stderr, "ADDR %d: index %ld (target %ld)\n", j,
                    addrs[j].iae_index,
                    infoSetInt[curInterf].if_info.ent.if_index);
            if(addrs[j].iae_index == infoSetInt[curInterf].if_info.ent.if_index)
            {
              memcpy(&infoSetInt[curInterf].ip_addr, &addrs[j],
                     sizeof(addrs[j]));
              curInterf++;
              break;
            }
          }
          if(NT_SUCCESS(status))
            tdiFreeThingSet(addrs);
        }
      }
    }

    tdiFreeThingSet(entIDSet);

    if(NT_SUCCESS(status))
    {
      *infoSet       = infoSetInt;
      *numInterfaces = curInterf;
    }
    else
    {
      HeapFree(GetProcessHeap(), 0, infoSetInt);
    }

    return status;
  }
  else
  {
    tdiFreeThingSet(entIDSet);
    return ((NTSTATUS)0xC000009A);
  }
}

NTSTATUS
getInterfaceInfoByName(HANDLE tcpFile, char *name, IFInfo *info)
{
  IFInfo *ifInfo;
  DWORD numInterfaces;
  int i;
  NTSTATUS status = getInterfaceInfoSet(tcpFile, &ifInfo, &numInterfaces);

  if(NT_SUCCESS(status))
  {
    for(i = 0; i < numInterfaces; i++)
    {
      if(!strcmp((PCHAR)ifInfo[i].if_info.ent.if_descr, name))
      {
        memcpy(info, &ifInfo[i], sizeof(*info));
        break;
      }
    }

    HeapFree(GetProcessHeap(), 0, ifInfo);

    return i < numInterfaces ? 0 : 0xc0000001;
  }

  return status;
}

NTSTATUS
getInterfaceInfoByIndex(HANDLE tcpFile, DWORD index, IFInfo *info)
{
  IFInfo *ifInfo;
  DWORD numInterfaces;
  NTSTATUS status = getInterfaceInfoSet(tcpFile, &ifInfo, &numInterfaces);
  int i;

  if(NT_SUCCESS(status))
  {
    for(i = 0; i < numInterfaces; i++)
    {
      if(ifInfo[i].if_info.ent.if_index == index)
      {
        memcpy(info, &ifInfo[i], sizeof(*info));
        break;
      }
    }

    HeapFree(GetProcessHeap(), 0, ifInfo);

    return i < numInterfaces ? 0 : 0xc0000001;
  }

  return status;
}

NTSTATUS
getIPAddrEntryForIf(HANDLE tcpFile, char *name, DWORD index, IFInfo *ifInfo)
{
  NTSTATUS status = name ? getInterfaceInfoByName(tcpFile, name, ifInfo)
                         : getInterfaceInfoByIndex(tcpFile, index, ifInfo);

  if(!NT_SUCCESS(status))
  {
    fprintf(stderr, "getIPAddrEntryForIf returning %lx\n", status);
  }

  return status;
}

InterfaceIndexTable *
getInterfaceIndexTableInt(BOOL nonLoopbackOnly)
{
  DWORD numInterfaces, curInterface = 0;
  int i;
  IFInfo *ifInfo;
  InterfaceIndexTable *ret = 0;
  HANDLE tcpFile;
  NTSTATUS status = openTcpFile(&tcpFile, FILE_READ_DATA);

  if(NT_SUCCESS(status))
  {
    status = getInterfaceInfoSet(tcpFile, &ifInfo, &numInterfaces);

    fprintf(stderr, "InterfaceInfoSet: %08lx, %04lx:%08lx\n", status,
            ifInfo->entity_id.tei_entity, ifInfo->entity_id.tei_instance);

    if(NT_SUCCESS(status))
    {
      ret = (InterfaceIndexTable *)calloc(
          1, sizeof(InterfaceIndexTable) + (numInterfaces - 1) * sizeof(DWORD));

      if(ret)
      {
        ret->numAllocated = numInterfaces;
        fprintf(stderr, "NumInterfaces = %ld\n", numInterfaces);

        for(i = 0; i < numInterfaces; i++)
        {
          fprintf(stderr, "Examining interface %d\n", i);
          if(!nonLoopbackOnly || !isLoopback(tcpFile, &ifInfo[i].entity_id))
          {
            fprintf(stderr, "Interface %d matches (%ld)\n", i, curInterface);
            ret->indexes[curInterface++] = ifInfo[i].if_info.ent.if_index;
          }
        }

        ret->numIndexes = curInterface;
      }

      tdiFreeThingSet(ifInfo);
    }
    closeTcpFile(tcpFile);
  }

  return ret;
}

InterfaceIndexTable *
getInterfaceIndexTable(void)
{
  return getInterfaceIndexTableInt(FALSE);
}

#endif

/*
 * We need this in the Microsoft C/C++ port, as we're not using Pthreads, and
 * jeff insists on naming the threads at runtime. Apparently throwing exception
 * 1080890248 is only visible when running under a machine code monitor.
 *
 * -despair86 30/07/18
 */
#ifdef _MSC_VER
#include <windows.h>
#define EXCEPTION_SET_THREAD_NAME ((DWORD)0x406D1388)
typedef struct _THREADNAME_INFO
{
  DWORD dwType;     /* must be 0x1000 */
  LPCSTR szName;    /* pointer to name (in user addr space) */
  DWORD dwThreadID; /* thread ID (-1=caller thread) */
  DWORD dwFlags;    /* reserved for future use, must be zero */
} THREADNAME_INFO;

void
SetThreadName(DWORD dwThreadID, LPCSTR szThreadName)
{
  THREADNAME_INFO info;
  DWORD infosize;

  info.dwType     = 0x1000;
  info.szName     = szThreadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags    = 0;

  infosize = sizeof(info) / sizeof(DWORD);

  __try
  {
    RaiseException(EXCEPTION_SET_THREAD_NAME, 0, infosize, (DWORD *)&info);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
  }
}
#endif