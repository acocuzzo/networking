#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "util/statusor.h"

#define DO_IF_ERROR(x, err, action)                             \
  if ((x) == -1) {                                              \
    std::cerr << (err) << ": " << strerror(errno) << std::endl; \
    action;                                                     \
  }

#define CONTINUE_IF_ERROR(x, err) DO_IF_ERROR(x, err, continue)
#define CLOSE_IF_ERROR(x, err, sock) DO_IF_ERROR(x, err, close(sock); continue)

namespace {
constexpr char kPort[] = "3490";
constexpr std::size_t kMaxDataSize =
    100;  // max number of bytes we can get at once

util::Status send_file_name(int sock, std::string filename) {
  const std::size_t len = filename.size() + 1;
  if (::send(sock, &len, sizeof(std::size_t), 0) == -1) {
    ::close(sock);
    return util::Status(util::Status::Code::INTERNAL, "client:send len");
  }
  if (::send(sock, filename.c_str(), len, 0) == -1) {
    ::close(sock);
    return util::Status(util::Status::Code::INTERNAL, "client:send filename");
  }
  return util::Status::make_OK();
}

util::StatusOr<std::size_t> rcv_file_size(int sock) {
  int numbytes;
  std::size_t filesize;
  if ((numbytes = recv(sock, &filesize, sizeof(std::size_t), 0)) == -1) {
    return util::Status(util::Status::Code::INTERNAL,
                        "client: recv filelength");
  }
  std::cout << "received filesize of " << filesize << std::endl;
  return filesize;
}
}  // namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "usage:client hostname" << std::endl;
    return 1;
  }

  struct addrinfo hints;
  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *servinfo_ptr = nullptr;
  int rv;
  if ((rv = getaddrinfo(argv[1], kPort, &hints, &servinfo_ptr)) == -1) {
    std::cerr << "getaddrinfo" << strerror(errno) << std::endl;
    return 1;
  }
  std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> servinfo(
      servinfo_ptr, freeaddrinfo);
  struct addrinfo *p;
  int sockfd;
  for (p = servinfo.get(); p != nullptr; p = p->ai_next) {
    CONTINUE_IF_ERROR(
        (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)),
        "client:socket");
    CLOSE_IF_ERROR((connect(sockfd, p->ai_addr, p->ai_addrlen)),
                   "client: connect", sockfd);
    break;
  }
  if (p == nullptr) {
    std::cerr << "client: failed to connect" << strerror(errno) << std::endl;
    return 2;
  }
  std::string filename = argv[2];
  auto send_name_status = send_file_name(sockfd, filename);
  if (!send_name_status.ok()) {
    ::close(sockfd);
    std::cerr << send_name_status << std::endl;
  }
  auto filesize_or = rcv_file_size(sockfd);
  if (!filesize_or.ok()) {
    ::close(sockfd);
    std::cerr << filesize_or.error() << std::endl;
  }
  std::size_t filesize = std::move(*filesize_or);
  std::fstream new_file;
  std::string directory = "/home/anna/code/networking/client/";

  while (filesize > 0) {
    const std::size_t partsize = filesize < kMaxDataSize ? filesize : kMaxDataSize;
    std::vector<char> file_data(partsize);
    int received = recv(sockfd, file_data.data(), partsize, 0);
    if (received == -1){
        std::cerr << "client: rcv file" << std::endl;
        ::close(sockfd);
        return 1;
    }
    new_file.open((directory + filename), std::ios::out | std::ios::app);
    new_file.write(file_data.data(), file_data.size());
    new_file.close();
    std::cout << "client: received segment: " << std::endl;
    std::cout.write(file_data.data(), file_data.size());
    std::cout << std::endl;
        filesize -= received;
  }
  std::cout << "client: received" << filename << std::endl;
  ::close(sockfd);
  return 0;
}