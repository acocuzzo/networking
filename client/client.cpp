
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

#define DO_IF_ERROR(x, y, err, action)                          \
  if ((x) == (y)) {                                             \
    std::cerr << (err) << ": " << strerror(errno) << std::endl; \
    action;                                                     \
  }

#define RETURN_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, return 1)
#define RETURN_2_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, return 2)
#define CONTINUE_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, continue)
#define CLOSE_R_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); return 1)
#define CLOSE_C_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); continue)

namespace {
constexpr char kPort[] = "3490";
constexpr std::size_t kMaxDataSize =
    100;  // max number of bytes we can get at once
}  // namespace
// namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "usage:client hostname" << std::endl;
    return 1;
  }

  struct addrinfo hints;
  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *servinfo_ptr = nullptr;
  int rv;
  RETURN_IF_ERROR((rv = getaddrinfo(argv[1], kPort, &hints, &servinfo_ptr)), -1,
                  "client: getaddrinfo");
  std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> servinfo(
      servinfo_ptr, freeaddrinfo);

  struct addrinfo *p;
  int sockfd;
  for (p = servinfo.get(); p != nullptr; p = p->ai_next) {
    CONTINUE_IF_ERROR(
        (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)), -1,
        "client:socket");
    CLOSE_C_IF_ERROR((connect(sockfd, p->ai_addr, p->ai_addrlen)), -1,
                     "client: connect", sockfd);
    break;
  }
  RETURN_2_IF_ERROR((p), nullptr, "client: failed to connect");
  std::string filename = "sample.txt";

  // send length of filename
  std::size_t len = sizeof(filename);
  CLOSE_R_IF_ERROR((send(sockfd, &len, sizeof(std::size_t), 0)), -1,
                   "client:send len", sockfd);
  // send file name
  CLOSE_R_IF_ERROR((send(sockfd, &filename, len, 0)), -1,
                   "client:send filename", sockfd);

  // rcv filelength
  int numbytes;
  std::size_t filesize;
  RETURN_IF_ERROR((numbytes = recv(sockfd, &filesize, sizeof(std::size_t), 0)),
                  -1, "client: recv filelength");
  std::cout << "received filesize of " << filesize << std::endl;
  // check bytes received matches a size_t
  //assert(numbytes == filesize);
  std::vector<char> file_data(filesize);
  std::fstream new_file;
  std::string directory = "/home/anna/code/networking/client";
  new_file.open((directory + filename), std::ios::out | std::ios::app);
  while (new_file.is_open()) {
    if (filesize <= kMaxDataSize) {
      RETURN_IF_ERROR((numbytes = recv(sockfd, &file_data, kMaxDataSize, 0)),
                      -1, "client: recv filedata");
      new_file << file_data.data();
      new_file.close();
    } else {
      std::size_t overflow = filesize - kMaxDataSize;
      int start = 0;
      while (overflow > 0) {
        if (overflow > 100) {
          RETURN_IF_ERROR(
              (numbytes = recv(sockfd, &(file_data[start]), kMaxDataSize, 0)),
              -1, "client: recv file segment");
          new_file << &(file_data[start]);
          overflow -= kMaxDataSize;
          start += kMaxDataSize;
        } else {
          RETURN_IF_ERROR(
              (numbytes = recv(sockfd, &(file_data[start]), overflow, 0)), -1,
              "client: recv file segment last");
          new_file << &(file_data[start]);
          new_file.close();
        }
      }
    }
  }
  std::cout << "client: received" << filename << std::endl;
  close(sockfd);
  return 0;
}