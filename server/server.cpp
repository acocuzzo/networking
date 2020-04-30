
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

#define DO_IF_ERROR(x, y, err, action)                          \
  if ((x) == (y)) {                                             \
    std::cerr << (err) << ": " << strerror(errno) << std::endl; \
    action;                                                     \
  }

#define EXIT_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, exit(1))
#define CONTINUE_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, continue)
#define CLOSE_C_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); continue)
#define CLOSE_E_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); exit(1))

namespace {
constexpr char kPort[] = "3490";
constexpr std::size_t kBacklog = 100;
constexpr std::size_t kMaxDataSize = 100;

std::vector<char> rcv_filename(int sock) {
  int numbytes;
  std::size_t len;
  EXIT_IF_ERROR((numbytes = recv(sock, &len, sizeof(std::size_t), 0)), -1,
                "client: recv");
  assert(numbytes == sizeof(std::size_t));
  std::cout << "received length of file name" << len << std::endl;
  std::vector<char> filename(len);
  EXIT_IF_ERROR((numbytes = recv(sock, filename.data(), len, 0)), -1,
                "client: recv");
  assert(numbytes == len);
  return filename;
}
std::vector<char> get_file_data(std::string filename) {
  std::vector<char> vec;
  std::ifstream file;
  file.open(filename, std::ifstream::in | std::ifstream::binary);
  file.seekg(0, std::ios::end);
  std::streampos length(file.tellg());
  if (length) {
    file.seekg(0, std::ios::beg);
    vec.resize(static_cast<std::size_t>(length));
    file.read(&vec.front(), static_cast<std::size_t>(length));
  }

  return vec;
}

std::size_t send_file_len(int sock, std::vector<char> file_data) {
  std::size_t filesize = file_data.size();
  std::cout << "filesize is " << filesize << std::endl;
  CLOSE_E_IF_ERROR((send(sock, &filesize, sizeof(std::size_t), 0)), -1,
                   "Server:send", sock);
  return filesize;
}
void send_segment(int sock, std::vector<char> file_data, std::size_t filesize) {
  if (filesize < kMaxDataSize) {
    CLOSE_E_IF_ERROR((send(sock, file_data.data(), filesize, 0)), -1,
                     "Server:send file segment last", sock);

    std::cout << "sent last segment starting with" << file_data[0] << std::endl;
  } else {
    CLOSE_E_IF_ERROR((send(sock, file_data.data(), kMaxDataSize, 0)), -1,
                     "Server:send file segment", sock);
    std::cout << "sent file data starting with" << file_data[0] << std::endl;
    file_data.erase(file_data.begin(), file_data.begin() + kMaxDataSize);
    filesize -= kMaxDataSize;
    send_segment(sock, file_data, filesize);
  }
}
/*
std::vector<char> slice_vec(std::vector<char> const &vec, int start,
                            int length) {
  auto first = vec.cbegin() + start;
  auto last = vec.cbegin() + start + length;
  std::vector<char> vec_slice(first, last);
  return vec_slice;
}
*/
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
    EXIT_IF_ERROR(
        ((::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))),
        -1, "Server error: setsockopt()");
    CLOSE_C_IF_ERROR((::bind(sockfd, p->ai_addr, p->ai_addrlen)), -1,
                     "Server error: bind", sockfd);
    break;
  }

  EXIT_IF_ERROR((p), nullptr, "server failed to bind");
  EXIT_IF_ERROR((::listen(sockfd, kBacklog)), -1, "server error: listen");

  std::cout << "server waiting for connections..." << std::endl;

  std::string directory = "/home/anna/code/networking/server/";
  while (true) {
    const int new_fd = accept(sockfd, nullptr, nullptr);
    CONTINUE_IF_ERROR((new_fd), -1, "server: error on accept");

    std::vector<char> filename = rcv_filename(new_fd);
    std::cout << "received filename" << filename.data() << std::endl;
    std::vector<char> file_data = get_file_data(directory + filename.data());
    std::size_t filesize = send_file_len(new_fd, file_data);

    send_segment(new_fd, file_data, filesize);
    ::close(new_fd);
  }
  ::close(sockfd);
  return 0;
}