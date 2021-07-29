#pragma once
#ifdef __APPLE__
#include <string>
#include <string_view>
#include <Foundation/Foundation.h>

static std::string_view
DataAsStringView(NSData* data)
{
  return std::string_view{reinterpret_cast<const char*>(data.bytes), data.length};
}

static NSData*
StringViewToData(std::string_view data)
{
  const char* ptr = data.data();
  const size_t sz = data.size();
  return [NSData dataWithBytes:ptr length:sz];
}

static NSString*
StringToNSString(std::string data)
{
  NSData* ptr = StringViewToData(std::string_view{data});
  return [[NSString alloc] initWithData:ptr encoding:NSUTF8StringEncoding];
}

static std::string
NSStringToString(NSString* str)
{
  return std::string{[str UTF8String]};
}

static std::string
NSObjectToString(NSObject* obj)
{
  return NSStringToString([NSString stringWithFormat:@"%@", obj]);
}

#endif
