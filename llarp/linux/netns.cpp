#if defined(ANDROID) || NETNS == 0

#else
#include <asm/types.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "netns.hpp"
#include <util/logger.hpp>
#ifndef MS_REC
#define MS_REC (16384)
#endif
#include <llarp/util/fs.hpp>

namespace llarp
{
  namespace GNULinux
  {
    static const char netns_rundir[] = "/var/run/netns";
    static const char netns_etcdir[] = "/etc/netns";

    static bool
    GetCGroups2MountPoint(fs::path& cgroups2_mount)
    {
      std::string mountpoint;
      std::ifstream inf;
      inf.open("/proc/mounts");
      if (!inf.is_open())
      {
        llarp::LogError("failed to open /proc/mounts");
        return false;
      }
      std::string line;
      while (std::getline(inf, line))
      {
        std::string part;
        std::stringstream parts;
        parts.str(line);
        // discard
        std::getline(parts, part);
        // mount point
        std::getline(parts, part);
        mountpoint = part;
        // type
        std::getline(parts, part);
        if (part == "cgroup2")
        {
          // found cgroup2 mountpoint
          cgroups2_mount = mountpoint;
          return true;
        }
      }
      llarp::LogError("cannot find cgroups2 in /proc/mounts");
      return false;
    }

    static bool
    GetNetNS(std::string& netns)
    {
      auto nfd = open("/proc/self/ns/net", O_RDONLY);
      if (nfd < 0)
      {
        llarp::LogError("Failed to get our own netns, could not open /proc/self/ns/net");
        return false;
      }
      struct stat netst;
      if (::fstat(nfd, &netst) < 0)
      {
        close(nfd);
        llarp::LogError("stat of netns failed: ", strerror(errno));
        return false;
      }
      close(nfd);
      fs::path run_dir = netns_rundir;
      bool foundIt = false;
      // find corrosponding file for netns
      llarp::util::IterDir(run_dir, [&](const fs::path& f) -> bool {
        struct stat fst;
        if (::stat(f.string().c_str(), &fst) >= 0)
        {
          if (fst.st_dev == netst.st_dev && fst.st_ino == netst.st_ino)
          {
            // found it
            foundIt = true;
            netns = f.filename().string();
            // break iteration
            return false;
          }
        }
        // continue iteration
        return true;
      });
      return foundIt;
    }

    static bool
    GetVRFPath(std::string& path)
    {
      char p[256] = {0};
      snprintf(p, sizeof(p), "/proc/%d/cgroup", getpid());
      std::ifstream inf;
      inf.open(p);
      if (!inf.is_open())
      {
        llarp::LogError("could not open '", p, "': ", strerror(errno));
        return false;
      }
      path = "";
      std::string line;
      while (std::getline(inf, line))
      {
        auto pos = line.find("::/");
        if (pos != std::string::npos)
        {
          line = line.substr(pos + 2);
          pos = line.find("/vrf");
          if (pos != std::string::npos)
          {
            path = line.substr(pos);
            if (path == "/")
              path = "";
          }
          break;
        }
      }
      return true;
    }

    static bool
    ResetVRF()
    {
      fs::path cgroups2_mount;
      if (!GetCGroups2MountPoint(cgroups2_mount))
      {
        llarp::LogError("could not find cgroup2 mount point, is it mounted?");
        return false;
      }
      std::string netns;
      if (!GetNetNS(netns))
      {
        llarp::LogError("could not get our netns: ", strerror(errno));
        return false;
      }
      std::string vrfpath;
      if (!GetVRFPath(vrfpath))
      {
        llarp::LogError("could not determine vrf cgroup path: ", strerror(errno));
        return false;
      }
      fs::path cgroup_path = cgroups2_mount / vrfpath / netns / "vrf" / "default";
      std::error_code ec;
      if (!fs::exists(cgroup_path, ec))
      {
        if (!fs::create_directories(cgroup_path, ec))
        {
          llarp::LogError("could not create '", cgroup_path.string(), "': ", ec);
          return false;
        }
      }
      else if (ec)
      {
        llarp::LogError("Could not check '", cgroup_path.string(), "': ", ec);
        return false;
      }
      cgroup_path /= "cgroup.procs";
      auto fd = open(cgroup_path.string().c_str(), O_RDWR | O_APPEND);
      if (fd < 0)
      {
        llarp::LogError("could not open '", cgroup_path.string(), "': ", strerror(errno));
        return false;
      }
      bool success = true;
      std::string pid = std::to_string(getpid());
      if (write(fd, pid.c_str(), pid.size()) < 0)
      {
        llarp::LogError("failed to join cgroup");
        success = false;
      }
      close(fd);
      return success;
    }

    /// bind network namespace paths into /etc/
    static bool
    BindNetworkNS(const char* name)
    {
      fs::path etc_dir = netns_etcdir;
      etc_dir /= name;
      std::error_code ec;
      if (!fs::exists(etc_dir, ec))
      {
        errno = 0;
        llarp::LogInfo(etc_dir, " does not exist, skipping");
        return true;
      }
      bool didFail = false;
      llarp::util::IterDir(etc_dir, [&](const fs::path& f) -> bool {
        if (fs::is_regular_file(f))
        {
          fs::path netns_path = "/etc";
          netns_path /= f.filename();
          if (mount(f.string().c_str(), netns_path.string().c_str(), "none", MS_BIND, nullptr) < 0)
          {
            llarp::LogError(
                "failed to bind '",
                f.string(),
                "' to '",
                netns_path.string(),
                "': ",
                strerror(errno));
            didFail = true;
          }
        }
        // continue iteration
        return true;
      });
      return !didFail;
    }

    static void
    DropCap()
    {
      if (getuid() != 0 && geteuid() != 0)
      {
        cap_t capabilities;
        cap_value_t net_admin = CAP_NET_ADMIN;
        cap_flag_t inheritable = CAP_INHERITABLE;
        cap_flag_value_t is_set;

        capabilities = cap_get_proc();
        if (!capabilities)
          exit(EXIT_FAILURE);
        if (cap_get_flag(capabilities, net_admin, inheritable, &is_set) != 0)
          exit(EXIT_FAILURE);

        if (is_set == CAP_CLEAR)
        {
          if (cap_clear(capabilities) != 0)
            exit(EXIT_FAILURE);
          if (cap_set_proc(capabilities) != 0)
            exit(EXIT_FAILURE);
        }
        cap_free(capabilities);
      }
    }

    bool
    NetNSSwitch(const char* name)
    {
      fs::path netns_path = netns_rundir;
      netns_path /= name;
      auto nsfd = open(netns_path.string().c_str(), O_RDONLY | O_CLOEXEC);
      if (nsfd < 0)
      {
        llarp::LogError("Failed to open network namespace '", name, "': ", strerror(errno));
        return false;
      }
      if (setns(nsfd, CLONE_NEWNET) < 0)
      {
        llarp::LogError("Failed to enter network namespace '", name, "': ", strerror(errno));
        close(nsfd);
        return false;
      }
      close(nsfd);
      if (unshare(CLONE_NEWNS) < 0)
      {
        llarp::LogError("unshare failed: ", strerror(errno));
        return false;
      }
      // dont let any mount points prop back to parent
      // iproute2 source does this
      if (mount("", "/", "none", MS_SLAVE | MS_REC, nullptr))
      {
        llarp::LogError("mount --make-rslave failed: ", strerror(errno));
        return false;
      }
      unsigned long mountflags = 0;
      // ensaure /sys not mounted
      if (umount2("/sys", MNT_DETACH) < 0)
      {
        struct statvfs fsstat;
        if (statvfs("/sys", &fsstat) == 0)
        {
          if (fsstat.f_flag & ST_RDONLY)
            mountflags = MS_RDONLY;
        }
      }
      // mount sysfs for our namespace
      if (mount(name, "/sys", "sysfs", mountflags, nullptr) < 0)
      {
        llarp::LogError("failed to mount sysfs: ", strerror(errno));
        return false;
      }
      if (!BindNetworkNS(name))
      {
        llarp::LogError("failed to bind namespace directories");
        return false;
      }
      if (!ResetVRF())
      {
        llarp::LogError("failed to reset vrf");
        return false;
      }
      DropCap();
      return true;
    }
  }  // namespace GNULinux
}  // namespace llarp

#endif
