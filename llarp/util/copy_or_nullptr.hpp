#pragma once
#include <memory>

template <typename T>
static constexpr std::unique_ptr<T>
copy_or_nullptr(const std::unique_ptr<T>& other)
{
  if (other)
    return std::make_unique<T>(*other);
  return nullptr;
}
