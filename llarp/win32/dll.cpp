#include "dll.hpp"
#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>

namespace llarp::win32
{
  namespace
  {
    auto cat = log::Cat("win32-dll");
  }
  DLL::DLL(std::string dll) : m_Handle{LoadLibraryA(dll.c_str())}
  {
    if (not m_Handle)
      throw win32::error{fmt::format("failed to load '{}'", dll)};
    log::info(cat, "loaded '{}'", dll);
  }

  DLL::~DLL()
  {
    FreeLibrary(m_Handle);
  }
}  // namespace llarp::win32
