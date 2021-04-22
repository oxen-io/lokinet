#include "shell.hpp"

#if defined(ENABLE_SHELLHOOKS)
#include <util/thread_pool.hpp>
#include <util/logger.hpp>
#include <sys/wait.h>
#include <unistd.h>
#if !defined(__linux__) || !defined(_GNU_SOURCE)
// Not all systems declare this variable
extern char** environ;
#endif
#if defined(Darwin)
#include <crt_externs.h>
#endif
#endif

namespace llarp
{
  namespace hooks
  {
#if defined(ENABLE_SHELLHOOKS)
    struct ExecShellHookBackend : public IBackend,
                                  public std::enable_shared_from_this<ExecShellHookBackend>
    {
      thread::ThreadPool m_ThreadPool;

      std::vector<std::string> _args;
      std::vector<char*> args;

      ExecShellHookBackend(std::string script) : m_ThreadPool(1, 1000, "exechook")
      {
        do
        {
          const auto idx = script.find_first_of(' ');
          std::string sub;
          if (idx == std::string::npos)
            sub = script;
          else
            sub = script.substr(0, idx);
          _args.emplace_back(std::move(sub));
          args.push_back((char*)_args.back().c_str());
          script = script.substr(idx + 1);
        } while (script.find_first_of(' ') != std::string::npos);
        args.push_back(nullptr);
        LogInfo("make hook ", args.size());
      }

      ~ExecShellHookBackend()
      {
        m_ThreadPool.shutdown();
      }

      bool
      Start() override
      {
        m_ThreadPool.start();
        return true;
      }

      bool
      Stop() override
      {
        m_ThreadPool.stop();
        return true;
      }

      char*
      Exe() const
      {
        return args[0];
      }

      char* const*
      Args() const
      {
        return args.data();
      }

      void
      NotifyAsync(std::unordered_map<std::string, std::string> params) override;
    };

    struct ExecShellHookJob
    {
      std::vector<std::string> m_env;
      std::vector<char*> _m_env;
      std::shared_ptr<ExecShellHookBackend> m_Parent;

      ExecShellHookJob(
          std::shared_ptr<ExecShellHookBackend> b,
          const std::unordered_map<std::string, std::string> env)
          : m_Parent(b)
      {
#if defined(Darwin)
        char** ptr = *_NSGetEnviron();
#else
        char** ptr = environ;
#endif
        do
        {
          m_env.emplace_back(*ptr);
          ++ptr;
        } while (ptr && *ptr);
        for (const auto& item : env)
          m_env.emplace_back(item.first + "=" + item.second);
        for (const auto& item : m_env)
          _m_env.push_back((char*)item.c_str());
        _m_env.push_back(nullptr);
      }

      char* const*
      Env()
      {
        return _m_env.data();
      }

      void
      Exec()
      {
        std::thread t([&]() {
          int result = 0;
          const pid_t child = ::fork();
          if (child == -1)
            return;
          if (child)
            ::waitpid(child, &result, 0);
          else
            ::execve(m_Parent->Exe(), m_Parent->Args(), Env());
        });
        t.join();
      }
    };

    void
    ExecShellHookBackend::NotifyAsync(std::unordered_map<std::string, std::string> params)
    {
      auto job = std::make_shared<ExecShellHookJob>(shared_from_this(), std::move(params));

      m_ThreadPool.addJob(std::bind(&ExecShellHookJob::Exec, job));
    }

    Backend_ptr
    ExecShellBackend(std::string execFilePath)
    {
      Backend_ptr ptr = std::make_shared<ExecShellHookBackend>(execFilePath);
      if (!ptr->Start())
        return nullptr;
      return ptr;
    }
#else
    Backend_ptr ExecShellBackend(std::string)
    {
      return nullptr;
    }
#endif
  }  // namespace hooks
}  // namespace llarp
