#include <hook/shell.hpp>
#if defined(_WIN32)
/** put win32 stuff here */
#else
#include <util/threadpool.h>
#include <util/logger.hpp>
#include <sys/wait.h>
#include <unistd.h>
#if !defined(__linux__) || !defined(_GNU_SOURCE)
// Not all systems declare this variable
extern char **environ;
#endif
#endif
#if defined(Darwin)
#include <crt_externs.h>
#endif

namespace llarp
{
  namespace hooks
  {
#if defined(_WIN32)
    Backend_ptr ExecShellBackend(std::string)
    {
      return nullptr;
    }
#else
    struct ExecShellHookBackend
        : public IBackend,
          public std::enable_shared_from_this< ExecShellHookBackend >
    {
      llarp_threadpool *m_ThreadPool;

      std::vector< std::string > _args;
      std::vector< char * > args;

      ExecShellHookBackend(std::string script)
          : m_ThreadPool(llarp_init_threadpool(1, script.c_str()))
      {
        do
        {
          const auto idx = script.find_first_of(' ');
          std::string sub;
          if(idx == std::string::npos)
            sub = script;
          else
            sub = script.substr(0, idx);
          _args.emplace_back(std::move(sub));
          args.push_back((char *)_args.back().c_str());
          script = script.substr(idx + 1);
        } while(script.find_first_of(' ') != std::string::npos);
        args.push_back(nullptr);
        LogInfo("make hook ", args.size());
      }

      ~ExecShellHookBackend()
      {
        llarp_threadpool_stop(m_ThreadPool);
        llarp_free_threadpool(&m_ThreadPool);
      }

      bool
      Start() override
      {
        llarp_threadpool_start(m_ThreadPool);
        return true;
      }

      bool
      Stop() override
      {
        llarp_threadpool_stop(m_ThreadPool);
        return true;
      }

      char *
      Exe() const
      {
        return args[0];
      }

      char *const *
      Args() const
      {
        return args.data();
      }

      void
      NotifyAsync(
          std::unordered_map< std::string, std::string > params) override;
    };

    struct ExecShellHookJob
    {
      std::vector< std::string > m_env;
      std::vector< char * > _m_env;
      std::shared_ptr< ExecShellHookBackend > m_Parent;

      ExecShellHookJob(std::shared_ptr< ExecShellHookBackend > b,
                       const std::unordered_map< std::string, std::string > env)
          : m_Parent(b)
      {
#if defined(Darwin)
        char **ptr = *_NSGetEnviron();
#else
        char **ptr = environ;
#endif
        do
        {
          m_env.emplace_back(*ptr);
          ++ptr;
        } while(ptr && *ptr);
        for(const auto &item : env)
          m_env.emplace_back(item.first + "=" + item.second);
        for(const auto &item : m_env)
          _m_env.push_back((char *)item.c_str());
        _m_env.push_back(nullptr);
      }

      char *const *
      Env()
      {
        return _m_env.data();
      }

      static void
      Exec(std::shared_ptr< ExecShellHookJob > self)
      {
        std::thread t([&]() {
          int result        = 0;
          const pid_t child = ::fork();
          if(child == -1)
            return;
          if(child)
            ::waitpid(child, &result, 0);
          else
            ::execve(self->m_Parent->Exe(), self->m_Parent->Args(),
                     self->Env());
        });
        t.join();
      }
    };

    void
    ExecShellHookBackend::NotifyAsync(
        std::unordered_map< std::string, std::string > params)
    {
      auto job = std::make_shared< ExecShellHookJob >(shared_from_this(),
                                                      std::move(params));
      m_ThreadPool->QueueFunc([=]() { ExecShellHookJob::Exec(job); });
    }

    Backend_ptr
    ExecShellBackend(std::string execFilePath)
    {
      Backend_ptr ptr = std::make_shared< ExecShellHookBackend >(execFilePath);
      if(!ptr->Start())
        return nullptr;
      return ptr;
    }
#endif
  }  // namespace hooks
}  // namespace llarp
