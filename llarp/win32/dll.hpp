#pragma once
#include <windows.h>
#include "exception.hpp"
#include <llarp/util/str.hpp>

namespace llarp::win32
{
  class DLL
  {
    const HMODULE m_Handle;

   protected:
    /// given a name of a function pointer find it and put it into `func`
    /// throws if the function does not exist in the DLL we openned.
    template <typename Func_t>
    void
    init(std::string name, Func_t& func)
    {
      auto ptr = GetProcAddress(m_Handle, name.c_str());
      if (not ptr)
        throw win32::error{fmt::format("function '{}' not found", name)};
      func = reinterpret_cast<Func_t&>(ptr);
    }

   public:
    DLL(std::string dll);

    virtual ~DLL();
  };
}  // namespace llarp::win32
