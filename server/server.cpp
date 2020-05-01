
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>

#include "statusor.h"

#define DO_IF_ERROR(x, y, err, action)                          \
  if ((x) == (y)) {                                             \
    std::cerr << (err) << ": " << strerror(errno) << std::endl; \
    action;                                                     \
  }

#define CONTINUE_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, continue)
#define CLOSE_C_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); continue)
#define CLOSE_E_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); exit(1))

namespace {
constexpr char kPort[] = "3490";
constexpr std::size_t kBacklog = 100;
constexpr std::size_t kMaxDataSize = 100;

StatusOr<std::vector<char>> rcv_filename(int sock) {
  int numbytes;
  std::size_t len;
  if ((numbytes = recv(sock, &len, sizeof(std::size_t), 0)) == -1)
  {
    return Status(Status::Code::INTERNAL, "client: recv");
  }
  assert(numbytes == sizeof(std::size_t));
  std::cout << "received length of file name" << len << std::endl;
  std::vector<char> filename(len);
  if ((numbytes = recv(sock, filename.data(), len, 0)) == -1)
  {
    return Status(Status::Code::INTERNAL, "client: recv");
  }
  assert(numbytes == len);
  return filename;
}


Status send_file_len(int sock, std::size_t filesize) {
  std::cout << "filesize is " << filesize << std::endl;
  if (send(sock, &filesize, sizeof(std::size_t), 0) == -1)
  {
    return Status(Status::Code::INTERNAL, "Server::send");
  }
  return Status::OK();
}
}  // namespace

int main(void) {
  struct addrinfo hints;  // info to fill in
  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  // will point to the results
  int rv;
  struct addrinfo *servinfo_ptr;  // will point to the results
  if ((rv = getaddrinfo(nullptr, kPort, &hints, &servinfo_ptr) != 0)) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    return 1;
  }
  std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> servinfo(
      servinfo_ptr, freeaddrinfo);

  int sockfd;  // listen on sockfd, new conn on new_fd
  struct addrinfo *p;
  for (p = servinfo.get(); p != nullptr; p = p->ai_next) {
    CONTINUE_IF_ERROR(
        (sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)), -1,
        "Server error: socket()");
    const int yes = 1;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      std::cerr << "Server error: setsockopt()" << std::endl;
      return -1;
    }
    CLOSE_C_IF_ERROR((::bind(sockfd, p->ai_addr, p->ai_addrlen)), -1,
                     "Server error: bind", sockfd);
    break;
  }

  if (p == nullptr || ::listen(sockfd, kBacklog) == -1)
  {
    return 1;
  }

  std::cout << "server waiting for connections..." << std::endl;
  const std::string directory = "/home/anna/code/networking/server/";
  while (true) {
    const int new_fd = ::accept(sockfd, nullptr, nullptr);
    CONTINUE_IF_ERROR(new_fd, -1, "server: error on accept");

    auto filename_or = rcv_filename(new_fd);
    if (!filename_or.ok())
    {
      ::close(new_fd);
      continue;
    }

    std::vector<char> filename = std::move(*filename_or);
    std::cout << "received filename" << filename.data() << std::endl;
    std::ifstream file;
    file.open(filename.data(), std::ifstream::in | std::ifstream::binary);
    file.seekg(0, std::ios::end);
    std::size_t filesize(file.tellg());
    file.seekg(0, std::ios::beg);
    Status status = send_file_len(new_fd, filesize);
    if (!status.ok())
    {
      ::close(new_fd);
      continue;
    }

    const std::size_t chunkSize = filesize < kMaxDataSize ? filesize : kMaxDataSize;
    std::vector<char> vec(chunkSize);
    while (filesize > 0)
    {
      const std::size_t partsize = filesize < chunkSize ? filesize : chunkSize;
      file.read(vec.data(), partsize);
      ssize_t sent = send(new_fd, vec.data(), partsize, 0);
      if (sent == -1)
      {
        break;
      }
      filesize -= sent;
    }
    ::close(new_fd);
  }
  ::close(sockfd);
  return 0;
}