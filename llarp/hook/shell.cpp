#include <hook/shell.hpp>
#include <util/thread_pool.hpp>
#include <util/logger.hpp>
#include <sys/wait.h>

namespace llarp
{
  namespace hooks
  {
    struct ExecShellHookJob
        : public std::enable_shared_from_this< ExecShellHookJob >
    {
      const std::string m_File;
      const std::unordered_map< std::string, std::string > m_env;
      ExecShellHookJob(
          const std::string f,
          const std::unordered_map< std::string, std::string > _env)
          : m_File(std::move(f)), m_env(std::move(_env))
      {
      }

      void
      Work()
      {
        char *const _args[] = {0};
        std::vector< std::string > _env(m_env.size() + 1);
        std::vector< char * > env;
        // copy environ
        const char * ptr = *environ;
        while(ptr)
          env.emplace_back(ptr++);
        // put in our variables
        for(const auto &item : m_env)
        {
          _env.emplace_back(item.first + "=" + item.second);
          env.emplace_back(_env.back().c_str());
        }
        env.emplace_back(nullptr);
        int status = 0;
        pid_t child_pid = ::fork();
        if(child_pid == -1)
        {
          LogError("fork failed: ", strerror(errno));
          errno = 0;
          return;
        }
        if(child_pid)
        {
          LogInfo(m_File, " spawned");
          ::waitpid(child_pid, &status, 0);
          LogInfo(m_File, " exit code: ", status);
        }
        else
          ::execvpe(m_File.c_str(), _args, env.data());          
      }
    };

    struct ExecShellHookBackend : public IBackend
    {
      ExecShellHookBackend(std::string script)
          : m_ThreadPool(1, 8), m_ScriptFile(std::move(script))
      {
      }

      llarp::thread::ThreadPool m_ThreadPool;
      const std::string m_ScriptFile;

      bool
      Start() override
      {
        return m_ThreadPool.start();
      }

      bool
      Stop() override
      {
        m_ThreadPool.stop();
        return true;
      }

      void
      NotifyAsync(
          std::unordered_map< std::string, std::string > params) override
      {
        auto job = std::make_shared< ExecShellHookJob >(m_ScriptFile,
                                                        std::move(params));
        m_ThreadPool.addJob(
            std::bind(&ExecShellHookJob::Work, job->shared_from_this()));
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