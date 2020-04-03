#if defined(__MINGW32__) && !defined(_WIN64)
/*
 * Contains routines missing from WS2_32.DLL until 2006, if yer using
 * Microsoft C/C++, then this code is irrelevant, as the official
 * Platform SDK already links against these routines in the correct
 * libraries.
 *
 * -despair86 30/07/18
 */

// these need to be in a specific order
#include <assert.h>
#include <stdbool.h>
#include <net/net.h>
#include <windows.h>
#include <iphlpapi.h>
#if WINNT_CROSS_COMPILE && !NTSTATUS
typedef LONG NTSTATUS;
#endif
#include "win32_intrnl.h"

const char*
inet_ntop(int af, const void* src, char* dst, size_t size)
{
  int address_length;
  DWORD string_length = size;
  struct sockaddr_storage sa;
  struct sockaddr_in* sin = (struct sockaddr_in*)&sa;
  struct sockaddr_in6* sin6 = (struct sockaddr_in6*)&sa;

  memset(&sa, 0, sizeof(sa));
  switch (af)
  {
    case AF_INET:
      address_length = sizeof(struct sockaddr_in);
      sin->sin_family = af;
      memcpy(&sin->sin_addr, src, sizeof(struct in_addr));
      break;

    case AF_INET6:
      address_length = sizeof(struct sockaddr_in6);
      sin6->sin6_family = af;
      memcpy(&sin6->sin6_addr, src, sizeof(struct in6_addr));
      break;

    default:
      return NULL;
  }

  if (WSAAddressToString((LPSOCKADDR)&sa, address_length, NULL, dst, &string_length) == 0)
  {
    return dst;
  }

  return NULL;
}

int
inet_pton(int af, const char* src, void* dst)
{
  int address_length;
  struct sockaddr_storage sa;
  struct sockaddr_in* sin = (struct sockaddr_in*)&sa;
  struct sockaddr_in6* sin6 = (struct sockaddr_in6*)&sa;

  switch (af)
  {
    case AF_INET:
      address_length = sizeof(struct sockaddr_in);
      break;

    case AF_INET6:
      address_length = sizeof(struct sockaddr_in6);
      break;

    default:
      return -1;
  }

  if (WSAStringToAddress((LPTSTR)src, af, NULL, (LPSOCKADDR)&sa, &address_length) == 0)
  {
    switch (af)
    {
      case AF_INET:
        memcpy(dst, &sin->sin_addr, sizeof(struct in_addr));
        break;

      case AF_INET6:
        memcpy(dst, &sin6->sin6_addr, sizeof(struct in6_addr));
        break;
    }
    return 1;
  }

  return 0;
}

typedef struct _InterfaceIndexTable
{
  DWORD numIndexes;
  IF_INDEX indexes[1];
} InterfaceIndexTable;

// windows 2000
// todo(despair86): implement IPv6 detection using
// the ipv6 preview stack/adv net pack from 1999/2001
DWORD FAR PASCAL
_GetAdaptersAddresses(
    ULONG Family,
    ULONG Flags,
    PVOID Reserved,
    PIP_ADAPTER_ADDRESSES pAdapterAddresses,
    PULONG pOutBufLen)
{
  InterfaceIndexTable* indexTable;
  IFInfo ifInfo;
  int i;
  ULONG ret, requiredSize = 0;
  PIP_ADAPTER_ADDRESSES currentAddress;
  PUCHAR currentLocation;
  HANDLE tcpFile;

  (void)(Family);
  if (!pOutBufLen)
    return ERROR_INVALID_PARAMETER;
  if (Reserved)
    return ERROR_INVALID_PARAMETER;

  indexTable = getInterfaceIndexTable();
  if (!indexTable)
    return ERROR_NOT_ENOUGH_MEMORY;

  ret = openTcpFile(&tcpFile, FILE_READ_DATA);
  if (!NT_SUCCESS(ret))
    return ERROR_NO_DATA;

  for (i = indexTable->numIndexes; i >= 0; i--)
  {
    if (NT_SUCCESS(getIPAddrEntryForIf(tcpFile, NULL, indexTable->indexes[i], &ifInfo)))
    {
      /* The whole struct */
      requiredSize += sizeof(IP_ADAPTER_ADDRESSES);

      /* Friendly name */
      if (!(Flags & GAA_FLAG_SKIP_FRIENDLY_NAME))
        requiredSize += strlen((char*)ifInfo.if_info.ent.if_descr) + 1;  // FIXME

      /* Adapter name */
      requiredSize += strlen((char*)ifInfo.if_info.ent.if_descr) + 1;

      /* Unicast address */
      if (!(Flags & GAA_FLAG_SKIP_UNICAST))
        requiredSize += sizeof(IP_ADAPTER_UNICAST_ADDRESS);

      /* FIXME: Implement multicast, anycast, and dns server stuff */

      /* FIXME: Implement dns suffix and description */
      requiredSize += 2 * sizeof(WCHAR);

      /* We're only going to implement what's required for XP SP0 */
    }
  }
#ifdef DEBUG
  fprintf(stderr, "size: %ld, requiredSize: %ld\n", *pOutBufLen, requiredSize);
#endif
  if (!pAdapterAddresses || *pOutBufLen < requiredSize)
  {
    *pOutBufLen = requiredSize;
    closeTcpFile(tcpFile);
    free(indexTable);
    return ERROR_BUFFER_OVERFLOW;
  }

  RtlZeroMemory(pAdapterAddresses, requiredSize);

  /* Let's set up the pointers */
  currentAddress = pAdapterAddresses;
  for (i = indexTable->numIndexes; i >= 0; i--)
  {
    if (NT_SUCCESS(getIPAddrEntryForIf(tcpFile, NULL, indexTable->indexes[i], &ifInfo)))
    {
      currentLocation = (PUCHAR)currentAddress + (ULONG_PTR)sizeof(IP_ADAPTER_ADDRESSES);

      /* FIXME: Friendly name */
      if (!(Flags & GAA_FLAG_SKIP_FRIENDLY_NAME))
      {
        currentAddress->FriendlyName = (PVOID)currentLocation;
        currentLocation += sizeof(WCHAR);
      }

      /* Adapter name */
      currentAddress->AdapterName = (PVOID)currentLocation;
      currentLocation += strlen((char*)ifInfo.if_info.ent.if_descr) + 1;

      /* Unicast address */
      if (!(Flags & GAA_FLAG_SKIP_UNICAST))
      {
        currentAddress->FirstUnicastAddress = (PVOID)currentLocation;
        currentLocation += sizeof(IP_ADAPTER_UNICAST_ADDRESS);
        currentAddress->FirstUnicastAddress->Address.lpSockaddr = (PVOID)currentLocation;
        currentLocation += sizeof(struct sockaddr);
      }

      /* FIXME: Implement multicast, anycast, and dns server stuff */

      /* FIXME: Implement dns suffix and description */
      currentAddress->DnsSuffix = (PVOID)currentLocation;
      currentLocation += sizeof(WCHAR);

      currentAddress->Description = (PVOID)currentLocation;
      currentLocation += sizeof(WCHAR);

      currentAddress->Next = (PVOID)currentLocation;
      /* Terminate the last address correctly */
      if (i == 0)
        currentAddress->Next = NULL;

      /* We're only going to implement what's required for XP SP0 */

      currentAddress = currentAddress->Next;
    }
  }

  /* Now again, for real this time */

  currentAddress = pAdapterAddresses;
  for (i = indexTable->numIndexes; i >= 0; i--)
  {
    if (NT_SUCCESS(getIPAddrEntryForIf(tcpFile, NULL, indexTable->indexes[i], &ifInfo)))
    {
      /* Make sure we're not looping more than we hoped for */
      assert(currentAddress);

      /* Alignment information */
      currentAddress->Length = sizeof(IP_ADAPTER_ADDRESSES);
      currentAddress->IfIndex = indexTable->indexes[i];

      /* Adapter name */
      strcpy(currentAddress->AdapterName, (char*)ifInfo.if_info.ent.if_descr);

      if (!(Flags & GAA_FLAG_SKIP_UNICAST))
      {
        currentAddress->FirstUnicastAddress->Length = sizeof(IP_ADAPTER_UNICAST_ADDRESS);
        currentAddress->FirstUnicastAddress->Flags = 0;  // FIXME
        currentAddress->FirstUnicastAddress->Next =
            NULL;  // FIXME: Support more than one address per adapter
        currentAddress->FirstUnicastAddress->Address.lpSockaddr->sa_family = AF_INET;
        memcpy(
            currentAddress->FirstUnicastAddress->Address.lpSockaddr->sa_data,
            &ifInfo.ip_addr.iae_addr,
            sizeof(ifInfo.ip_addr.iae_addr));
        currentAddress->FirstUnicastAddress->Address.iSockaddrLength =
            sizeof(ifInfo.ip_addr.iae_addr) + sizeof(USHORT);
        currentAddress->FirstUnicastAddress->PrefixOrigin = IpPrefixOriginOther;  // FIXME
        currentAddress->FirstUnicastAddress->SuffixOrigin = IpSuffixOriginOther;  // FIXME
        currentAddress->FirstUnicastAddress->DadState = IpDadStatePreferred;      // FIXME
        currentAddress->FirstUnicastAddress->ValidLifetime = 0xFFFFFFFF;          // FIXME
        currentAddress->FirstUnicastAddress->PreferredLifetime = 0xFFFFFFFF;      // FIXME
        currentAddress->FirstUnicastAddress->LeaseLifetime = 0xFFFFFFFF;          // FIXME
      }

      /* FIXME: Implement multicast, anycast, and dns server stuff */
      currentAddress->FirstAnycastAddress = NULL;
      currentAddress->FirstMulticastAddress = NULL;
      currentAddress->FirstDnsServerAddress = NULL;

      /* FIXME: Implement dns suffix, description, and friendly name */
      currentAddress->DnsSuffix[0] = UNICODE_NULL;
      currentAddress->Description[0] = UNICODE_NULL;
      currentAddress->FriendlyName[0] = UNICODE_NULL;

      /* Physical Address */
      memcpy(
          currentAddress->PhysicalAddress,
          ifInfo.if_info.ent.if_physaddr,
          ifInfo.if_info.ent.if_physaddrlen);
      currentAddress->PhysicalAddressLength = ifInfo.if_info.ent.if_physaddrlen;

      /* Flags */
      currentAddress->Flags = 0;  // FIXME

      /* MTU */
      currentAddress->Mtu = ifInfo.if_info.ent.if_mtu;

      /* Interface type */
      currentAddress->IfType = ifInfo.if_info.ent.if_type;

      /* Operational status */
      if (ifInfo.if_info.ent.if_operstatus >= IF_OPER_STATUS_CONNECTING)
        currentAddress->OperStatus = IfOperStatusUp;
      else
        currentAddress->OperStatus = IfOperStatusDown;

      /* We're only going to implement what's required for XP SP0 */

      /* Move to the next address */
      currentAddress = currentAddress->Next;
    }
  }

  closeTcpFile(tcpFile);
  free(indexTable);

  return NO_ERROR;
}
#endif
