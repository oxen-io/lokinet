#include <hook/shell.hpp>
#include <util/threadpool.h>
#include <util/logger.hpp>
#include <sys/wait.h>

namespace llarp
{
  namespace hooks
  {
    struct ExecShellHookJob
    {
      const std::string &m_File;
      const std::unordered_map< std::string, std::string > m_env;
      ExecShellHookJob(
          const std::string &f,
          const std::unordered_map< std::string, std::string > _env)
          : m_File(f), m_env(std::move(_env))
      {
      }

      static void
      Exec(void *user)
      {
        ExecShellHookJob *self = static_cast< ExecShellHookJob * >(user);
        char *const _args[]    = {0};
        std::vector< std::string > _env(self->m_env.size() + 1);
        std::vector< char * > env;
        // copy environ
        char **ptr = environ;
        do
        {
          env.emplace_back(*ptr);
          ++ptr;
        } while(ptr && *ptr);
        // put in our variables
        for(const auto &item : self->m_env)
        {
          _env.emplace_back(item.first + "=" + item.second);
          env.emplace_back(_env.back().c_str());
        }
        env.emplace_back(0);
        int status      = 0;
        pid_t child_pid = ::fork();
        if(child_pid == -1)
        {
          LogError("fork failed: ", strerror(errno));
          errno = 0;
          delete self;
          return;
        }
        if(child_pid)
        {
          LogInfo(self->m_File, " spawned");
          ::waitpid(child_pid, &status, 0);
          LogInfo(self->m_File, " exit code: ", status);
          delete self;
        }
        else
          ::execvpe(self->m_File.c_str(), _args, env.data());
      }
    };

    struct ExecShellHookBackend : public IBackend
    {
      llarp_threadpool *m_ThreadPool;
      const std::string m_ScriptFile;

      ExecShellHookBackend(std::string script)
          : m_ThreadPool(llarp_init_threadpool(1, script.c_str()))
          , m_ScriptFile(std::move(script))
      {
      }

      ~ExecShellHookBackend()
      {
        llarp_threadpool_stop(m_ThreadPool);
        llarp_threadpool_join(m_ThreadPool);
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
        llarp_threadpool_join(m_ThreadPool);
        return true;
      }

      void
      NotifyAsync(
          std::unordered_map< std::string, std::string > params) override
      {
        ExecShellHookJob *job =
            new ExecShellHookJob(m_ScriptFile, std::move(params));
        llarp_threadpool_queue_job(m_ThreadPool,
                                   {job, &ExecShellHookJob::Exec});
      }
    };

    Backend_ptr
    ExecShellBackend(std::string execFilePath)
    {
      Backend_ptr ptr = std::make_unique< ExecShellHookBackend >(execFilePath);
      if(!ptr->Start())
        return nullptr;
      return ptr;
    }
  }  // namespace hooks
}  // namespace llarp