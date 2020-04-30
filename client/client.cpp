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

#define EXIT_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, exit(1))
#define EXIT_2_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, exit(2))
#define CONTINUE_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, continue)
#define CLOSE_E_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); exit(1))
#define CLOSE_C_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); continue)

namespace {
constexpr char kPort[] = "3490";
constexpr std::size_t kMaxDataSize =
    100;  // max number of bytes we can get at once

void send_file_name(int sock, std::string filename){
  const std::size_t len = filename.size() + 1;
  CLOSE_E_IF_ERROR((send(sock, &len, sizeof(std::size_t), 0)), -1,
                   "client:send len", sock);
  CLOSE_E_IF_ERROR((send(sock, filename.c_str(), len, 0)), -1,
                   "client:send filename", sock);
}

std::size_t rcv_file_size(int sock){
  int numbytes;
  std::size_t filesize;
  EXIT_IF_ERROR((numbytes = recv(sock, &filesize, sizeof(std::size_t), 0)),
                  -1, "client: recv filelength");
  std::cout << "received filesize of " << filesize << std::endl;
  return filesize;
}
void rcv_file(int sock, std::string filename, std::size_t filesize) {
  int numbytes;
  std::fstream new_file;
  std::string directory = "/home/anna/code/networking/client/";
  if (filesize <= kMaxDataSize) {
    std::vector<char> file_data(filesize);
    EXIT_IF_ERROR((numbytes = recv(sock, file_data.data(), filesize, 0)), -1,
                  "client: recv filedata");

    new_file.open((directory + filename), std::ios::out | std::ios::app);
    new_file << file_data.data();
    new_file.close();
    std::cout << "client: received last segment" << file_data.data()
              << std::endl;
  } else {
    std::vector<char> file_segment(kMaxDataSize);
    EXIT_IF_ERROR((numbytes = recv(sock, file_segment.data(), kMaxDataSize, 0)),
                  -1, "client: recv file segment");

    new_file.open((directory + filename), std::ios::out | std::ios::app);
    new_file << file_segment.data();
    new_file.close();
    std::cout << "client: received segment" << file_segment.data() << std::endl;
    filesize -= kMaxDataSize;
    rcv_file(sock, filename, filesize);
  }
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
  EXIT_IF_ERROR((rv = getaddrinfo(argv[1], kPort, &hints, &servinfo_ptr)), -1,
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
  EXIT_2_IF_ERROR((p), nullptr, "client: failed to connect");

  send_file_name(sockfd, argv[2]);
  std::size_t filesize = rcv_file_size(sockfd);
  rcv_file(sockfd, argv[2], filesize);
  std::cout << "client: received" << argv[2] << std::endl;
  close(sockfd);
  return 0;
}