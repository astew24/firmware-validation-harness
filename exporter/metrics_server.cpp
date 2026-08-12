#include "metrics_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstdint>
#include <string>

namespace {

void send_all(int fd, const std::string& data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
    if (n <= 0) return;
    sent += static_cast<std::size_t>(n);
  }
}

std::string response(int code, const char* reason, const char* type, const std::string& body) {
  return "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\nContent-Type: " + type +
         "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

}  // namespace

bool serve_metrics(int port, const std::function<std::string()>& render,
                   const std::atomic<bool>& running) {
  std::signal(SIGPIPE, SIG_IGN);
  const int srv = ::socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) return false;
  int yes = 1;
  ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || ::listen(srv, 16) < 0) {
    ::close(srv);
    return false;
  }

  while (running.load()) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(srv, &fds);
    timeval tv{};
    tv.tv_usec = 200000;  // wake up regularly to notice shutdown
    if (::select(srv + 1, &fds, nullptr, nullptr, &tv) <= 0) continue;

    const int fd = ::accept(srv, nullptr, nullptr);
    if (fd < 0) continue;
    char buf[2048];
    const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    const std::string req = n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string();
    if (req.rfind("GET /metrics", 0) == 0) {
      send_all(fd, response(200, "OK", "text/plain; version=0.0.4; charset=utf-8", render()));
    } else if (req.rfind("GET / ", 0) == 0) {
      send_all(fd, response(200, "OK", "text/plain; charset=utf-8", "firmware validation harness: see /metrics\n"));
    } else {
      send_all(fd, response(404, "Not Found", "text/plain", "not found\n"));
    }
    ::close(fd);
  }
  ::close(srv);
  return true;
}
