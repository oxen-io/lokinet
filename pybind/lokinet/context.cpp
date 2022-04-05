#include "common.hpp"
#include <llarp.hpp>
#include <llarp/nodedb.hpp>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace lokinet
{
  class PyNodeDB : public llarp::NodeDB
  {
    py::function _load, _store, _del;

   protected:
    void
    AsyncRemoveManyFromDisk(std::unordered_set<llarp::RouterID> idents) const override
    {
      py::gil_scoped_acquire gil{};
      for (const auto& key : idents)
        _del(py::str{key.ToString()});
    }

   public:
    explicit PyNodeDB(py::function load, py::function store, py::function del)
        : _load{load}, _store{store}, _del{del}
    {}

    void
    LoadFromDisk() override
    {
      py::gil_scoped_acquire gil{};
      py::dict all{_load()};
      for (const auto& entry : all)
      {
        char* buf{};
        ssize_t len{};
        if (PyBytes_AsStringAndSize(entry.second.ptr(), &buf, &len) == -1)
          return;
        llarp::RouterContact rc{};
        llarp_buffer_t tmp{buf, static_cast<size_t>(len)};
        if (not rc.BDecode(&tmp))
          continue;
        if (rc.Verify(llarp::time_now_ms()))
          m_Entries.emplace(rc.pubkey, rc);
      }
    }

    void
    SaveToDisk() const override
    {
      py::gil_scoped_acquire gil{};
      std::array<byte_t, 1024> tmp;
      for (const auto& item : m_Entries)
      {
        llarp_buffer_t buf{tmp};
        if (item.second.rc.BEncode(&buf))
          _store(
              item.first.ToString(),
              py::bytes{
                  reinterpret_cast<const char*>(buf.base),
                  static_cast<size_t>(buf.cur - buf.base)});
      }
    }
  };

  struct PyContext;

  struct LokinetContext : public llarp::Context
  {
    using llarp::Context::Context;

    PyContext* const pyctx;

    explicit LokinetContext(PyContext* ctx) : llarp::Context{}, pyctx{ctx}
    {}
    ~LokinetContext() = default;

    std::shared_ptr<llarp::NodeDB>
    makeNodeDB() override;
  };

  struct PyContext
  {
    py::function load;
    py::function store;
    py::function del;
    LokinetContext* const ctx;
    lokinet_context* const _impl;

    PyContext() : ctx{new LokinetContext{this}}, _impl{::lokinet_context_cast(ctx)}
    {}

    ~PyContext()
    {
      py::gil_scoped_release gil{};
      lokinet_context_free(_impl);
    }

    void
    Start()
    {
      py::gil_scoped_release gil{};
      if (lokinet_context_start(_impl))
        throw std::runtime_error{"cannot start liblokinet context"};
    }

    void
    Stop()
    {
      py::gil_scoped_release gil{};
      lokinet_context_stop(_impl);
    }

    std::string
    Status()
    {
      switch (lokinet_status(_impl))
      {
        case 0:
          return "ready";
        case -1:
          return "building";
        case -2:
          return "deadlocked";
        case -3:
          return "stopped";
        default:
          return "unknown";
      }
    }

    void
    AddBootstrapRC(std::string_view data)
    {
      py::gil_scoped_release gil{};
      if (lokinet_add_bootstrap_rc(data.data(), data.size(), _impl))
        throw std::runtime_error{"failed to set boostrap data"};
    }

    bool
    WaitForReady(int N)
    {
      py::gil_scoped_release gil{};
      return lokinet_wait_for_ready(N, _impl) == 0;
    }

    std::string
    LocalAddress()
    {
      py::gil_scoped_release gil{};
      return lokinet_address(_impl);
    }

    void
    SetConfigOpt(std::string section, std::string key, std::string val)
    {
      py::gil_scoped_release gil{};
      lokinet_config_add_opt(lokinet_get_config(_impl), section.c_str(), key.c_str(), val.c_str());
    }

    std::tuple<std::string, int, int>
    ResolveOutbound(std::string remoteAddr, int timeout)
    {
      py::gil_scoped_release gil{};
      lokinet_stream_result result{};
      lokinet_outbound_stream(&result, remoteAddr.c_str(), nullptr, timeout, _impl);
      if (result.error)
        throw std::runtime_error{strerror(result.error)};
      return std::make_tuple(result.local_address, result.local_port, result.stream_id);
    }

    void
    CloseSocket(int id)
    {
      lokinet_close_stream(id, _impl);
    }
  };

  std::shared_ptr<llarp::NodeDB>
  LokinetContext::makeNodeDB()
  {
    if (pyctx->load and pyctx->load and pyctx->load)
      return std::make_shared<PyNodeDB>(pyctx->load, pyctx->store, pyctx->del);
    return std::make_shared<llarp::NodeDB>();
  }

  void
  Init_Context(py::module& mod)
  {
    py::class_<PyContext>(mod, "Context")
        .def(py::init<>())
        .def_readwrite("nodedb_load", &PyContext::load)
        .def_readwrite("nodedb_store", &PyContext::store)
        .def_readwrite("nodedb_del", &PyContext::del)
        .def("set_config_opt", &PyContext::SetConfigOpt)
        .def("start", &PyContext::Start)
        .def("add_bootstrap_rc", &PyContext::AddBootstrapRC)
        .def("stop", &PyContext::Stop)
        .def("status", &PyContext::Status)
        .def("wait_for_ready", &PyContext::WaitForReady)
        .def("localaddr", &PyContext::LocalAddress)
        .def("resolve", &PyContext::ResolveOutbound)
        .def("unresolve", &PyContext::CloseSocket);
  }

}  // namespace lokinet