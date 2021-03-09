#include <common.hpp>
#include <memory>
#include <llarp/util/logging/logger.hpp>

namespace llarp
{
  struct PyLogger
  {
    std::unique_ptr<LogSilencer> shutup;
  };

  void
  Logger_Init(py::module& mod)
  {
    py::class_<PyLogger>(mod, "LogContext")
        .def(py::init<>())
        .def_property(
            "shutup",
            [](PyLogger& self) { return self.shutup != nullptr; },
            [](PyLogger& self, bool shutup) {
              if (self.shutup == nullptr && shutup)
              {
                self.shutup = std::make_unique<LogSilencer>();
              }
              else if (self.shutup != nullptr && shutup == false)
              {
                self.shutup.reset();
              }
            });
  }
}  // namespace llarp
