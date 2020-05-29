#include <lokimq/lokimq.h>
#include <nlohmann/json.hpp>
#include <future>
#include <chrono>
#include <signal.h>
#include <curses.h>
#include <cstdio>

using namespace std::chrono_literals;

std::vector<std::string>
get_args(int argc, char** argv)
{
  std::vector<std::string> args;
  for (int arg = 0; arg < argc; arg++)
  {
    args.emplace_back(argv[arg]);
  }
  return args;
}

std::promise<bool> exit_promise;

void
HandleSignal(int)
{
  exit_promise.set_value(true);
}

struct Stats
{
  std::list<nlohmann::json> m_Data;
  std::string version;
  template <typename T>
  static auto
  speedof(const T& rate)
  {
    const std::array rates{"b", "Kb", "Mb", "Gb"};
    size_t idx = 0;
    uint64_t rate_int = 0;
    rate.get_to(rate_int);
    double rate_float = rate_int * 8;
    while (rate_float > 1024.0 and idx < rates.size())
    {
      rate_float /= 1024.0;
      idx++;
    }
    char buf[64] = {0};
    std::snprintf(buf, sizeof(buf), "%.2f%sps", rate_float, rates[idx]);
    return std::string(buf);
  }

  template <typename Visit_t>
  void
  ForEachLink(Visit_t visit) const
  {
    const auto& back = m_Data.back();
    const auto& links = back.at("links");
    {
      const auto& l = links.at("outbound");
      for (auto itr = l.begin(); itr != l.end(); ++itr)
      {
        visit(*itr);
      }
    }
    {
      const auto& l = links.at("inbound");
      for (auto itr = l.begin(); itr != l.end(); ++itr)
      {
        visit(*itr);
      }
    }
  }

  template <typename T>
  void
  ShowLinkSession(const T& session, WINDOW* win) const
  {
    std::stringstream ss;
    std::string addr;
    session["remoteAddr"].get_to(addr);
    ss << addr << "\t";
    ss << "[" << speedof(session["txRateCurrent"]) << "\ttx]\t";
    ss << "[" << speedof(session["rxRateCurrent"]) << "\trx]";
    const auto str = ss.str();
    waddstr(win, str.c_str());
  }

  void
  DisplayLinks(WINDOW* win) const
  {
    int y = 3;
    ForEachLink([&](const auto& link) {
      const auto& sessions = link.at("sessions");
      const auto& established = sessions.at("established");
      for (auto itr = established.begin(); itr != established.end(); ++itr)
      {
        wmove(win, y++, 1);
        ShowLinkSession(*itr, win);
      }
    });
  }

  void
  DisplayServices(WINDOW* win) const
  {
    (void)win;
  }

  void
  Update(WINDOW* win) const
  {
    wmove(win, 1, 1);
    waddstr(win, version.c_str());
    DisplayLinks(win);
    DisplayServices(win);
  }

  void
  AddSample(nlohmann::json data)
  {
    while (m_Data.size() > 64)
    {
      m_Data.pop_front();
    }
    m_Data.emplace_back(std::move(data));
    RecalcStats();
  }

  void
  RecalcStats()
  {
  }
};

void
UpdateUI(
    std::shared_ptr<lokimq::LokiMQ> lmq,
    lokimq::ConnectionID id,
    std::shared_ptr<Stats> stats,
    WINDOW* win)
{
  lmq->request(id, "llarp.status", [win, stats](bool success, std::vector<std::string> data) {
    wmove(win, 2, 1);
    if (not success)
    {
      waddstr(win, "request failed");
      wrefresh(win);
      return;
    }
    if (data.empty())
    {
      waddstr(win, "no data");
      wrefresh(win);
      return;
    }
    try
    {
      const auto j = nlohmann::json::parse(data.at(0));
      stats->AddSample(j);
    }
    catch (std::exception&)
    {
    }
    wclear(win);
    stats->Update(win);
    wrefresh(win);
  });
}

void
resizeHandler(int)
{
}

int
main(int argc, char* argv[])
{
  std::string connect_str = "tcp://127.0.0.1:1190/";
  const auto args = get_args(argc, argv);
  if (args.size() == 2)
  {
    connect_str = args[1];
  }
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);
  signal(SIGWINCH, resizeHandler);
  auto stats = std::make_shared<Stats>();
  auto lmq = std::make_shared<lokimq::LokiMQ>();
  lmq->start();
  lmq->connect_remote(
      connect_str,
      [lmq, stats](lokimq::ConnectionID id) {
        auto screen = ::initscr();
        if (screen == nullptr)
        {
          std::cout << "failed to init screen" << std::endl;
          exit_promise.set_value(false);
          return;
        }
        ::cbreak();
        ::noecho();
        std::stringstream ss;
        ss << "connected via " << id;
        const auto str = ss.str();
        waddstr(stdscr, str.c_str());
        wrefresh(stdscr);
        lmq->request(id, "llarp.version", [stats](bool success, std::vector<std::string> data) {
          wmove(stdscr, 1, 1);

          if (success and not data.empty())
          {
            stats->version = std::move(data[0]);
          }
          else
          {
            waddstr(stdscr, "failed to get version");
          }
          wrefresh(stdscr);
        });
        lmq->add_timer([lmq, stats, id]() { UpdateUI(lmq, id, stats, stdscr); }, 1s);
      },
      [](lokimq::ConnectionID, std::string_view fail) {
        std::cout << "failed to start lokinetmon: " << fail << std::endl;
        exit_promise.set_value(false);
      });
  auto ftr = exit_promise.get_future();
  if (ftr.get())
    return ::endwin();
  else
    return 0;
}
