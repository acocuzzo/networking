
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

#define RETURN_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, return 1)
#define CONTINUE_IF_ERROR(x, y, err) DO_IF_ERROR(x, y, err, continue)
#define CLOSE_C_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); continue)
#define CLOSE_R_IF_ERROR(x, y, err, sock) \
  DO_IF_ERROR(x, y, err, close(sock); return 1)

namespace {
constexpr char kPort[] = "3490";
constexpr std::size_t kBacklog = 100;
constexpr std::size_t kMaxDataSize = 100;

std::string receive_file_name(int sock){

return;
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
    RETURN_IF_ERROR(
        ((::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))),
        -1, "Server error: setsockopt()");
    CLOSE_C_IF_ERROR((::bind(sockfd, p->ai_addr, p->ai_addrlen)), -1,
                     "Server error: bind", sockfd);
    break;
  }

  RETURN_IF_ERROR((p), nullptr, "server failed to bind");
  RETURN_IF_ERROR((::listen(sockfd, kBacklog)), -1, "server error: listen");

  std::cout << "server waiting for connections..." << std::endl;

  std::string directory = "/home/anna/code/networking/server/";
  while (true) {
    const int new_fd = accept(sockfd, nullptr, nullptr);
    CONTINUE_IF_ERROR((new_fd), -1, "server: error on accept");


    // rcv length of file name from client
    int numbytes;
    std::size_t len;
    RETURN_IF_ERROR((numbytes = recv(new_fd, &len, sizeof(std::size_t), 0)), -1,
                    "client: recv");
    // make sure they sent a size_t type
    assert(numbytes == sizeof(std::size_t));
    std::cout << "received length of file name" << len << std::endl;
    // create a char vector to read into
    std::vector<char> filename(len);
    // rcv filename from client
    RETURN_IF_ERROR((numbytes = recv(new_fd, filename.data(), len, 0)), -1,
                    "client: recv");
    // make sure filename size matches expected length
    assert(numbytes == len);
    std::cout << "received filename" << filename.data() << std::endl;

    std::vector<char> file_data = get_file_data(directory + filename.data());
    // send length of the file
    std::size_t filesize = file_data.size();
    std::cout << "filesize is " << filesize << std::endl;
    CLOSE_R_IF_ERROR((send(new_fd, &filesize, sizeof(std::size_t), 0)), -1,
                     "Server:send", new_fd);
    // send the file
    // if file is less than or equal to maxdatasize, send in one chunk
    if (filesize <= kMaxDataSize) {
      CLOSE_R_IF_ERROR((send(new_fd, file_data.data(), filesize, 0)), -1,
                       "Server: sendfile kMaxDataSize", new_fd);
      std::cout << "sent file data starting with" << file_data[0] << std::endl;
    }
    // if file is greater than maxdatasize
    else {
      std::size_t overflow = filesize - kMaxDataSize;
      int start = 0;
      while (overflow > 0) {
        // if greater than maxdatasize by more than 100, send maxdatasize at a
        // time.
        if (overflow > 100) {
          CLOSE_R_IF_ERROR((send(new_fd, &(file_data.data()[start]), kMaxDataSize, 0)),
                           -1, "Server:send file segment", new_fd);
          
          std::cout << "sent file data starting with" << file_data[start] << std::endl;
          overflow -= kMaxDataSize;
          start += kMaxDataSize;
        }
        // if equal to or less than maxdatasize, send only overflow data
        else {
          CLOSE_R_IF_ERROR((send(new_fd, &(file_data.data()[start]), overflow, 0)), -1,
                           "Server:send file segment last", new_fd);
          
          std::cout << "sent last segment starting with" << file_data[start] << std::endl;
        }
      }
    }

    ::close(new_fd);
  }
  ::close(sockfd);
  return 0;
}