
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
//
#include <thread>
#include <mutex>

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
constexpr std::size_t kBacklog = 100;
constexpr std::size_t kMaxDataSize = 100;


util::StatusOr<std::vector<char>> rcv_filename(int sock) {
  int numbytes;
  std::size_t len;
  if ((numbytes = recv(sock, &len, sizeof(std::size_t), 0)) == -1) {
    return util::Status(util::Status::Code::INTERNAL, "svr: rcv filename len");
  }
  assert(numbytes == sizeof(std::size_t));
  std::cout << "received length of file name" << len << std::endl;
  std::vector<char> filename(len);
  if ((numbytes = recv(sock, filename.data(), len, 0)) == -1) {
    return util::Status(util::Status::Code::INTERNAL, "svr: rcv filename");
  }
  assert(numbytes == len);
  return filename;
}
/*
util::StatusOr<int> rcv_num_files(int sock) {
  int numbytes;
  int num_files;
  if ((numbytes = recv(sock, &num_files, sizeof(int), 0)) == -1) {
    return util::Status(util::Status::Code::INTERNAL, "svr: rcv num_files");
  }
  assert(numbytes == sizeof(int));
  std::cout <<  num_files << "will be requested" << std::endl;
  return num_files;
}*/

util::Status send_file_len(int sock, std::size_t filesize) {
  std::cout << "filesize is " << filesize << std::endl;
  if (send(sock, &filesize, sizeof(std::size_t), 0) == -1) {
    return util::Status(util::Status::Code::INTERNAL, "svr: send filesize");
  }
  return util::Status::make_OK();
}

void ftp(int new_fd, int t_id, std::mutex* mu) {
  auto filename_or = rcv_filename(new_fd);
  if (!filename_or.ok()) {
      ::close(new_fd);
      return;
   }
   std::vector<char> filename = std::move(*filename_or);

  {
    std::lock_guard<std::mutex> ifstream_lock(*mu);
    std::cout << "received filename" << filename.data() << std::endl;
  }
  std::ifstream file;
  const std::string directory = "/home/anna/code/networking/server/";
  file.open(directory + filename.data(),
            std::ifstream::in | std::ifstream::binary);
  file.seekg(0, std::ios::end);
  std::size_t filesize(file.tellg());
  file.seekg(0, std::ios::beg);
  util::Status status = send_file_len(new_fd, filesize);
  if (!status.ok()) {
    ::close(new_fd);
    return;
  }

  const std::size_t chunkSize =
      filesize < kMaxDataSize ? filesize : kMaxDataSize;
  while (filesize > 0) {
    const std::size_t partsize = filesize < chunkSize ? filesize : chunkSize;
    std::vector<char> vec(partsize);
    file.read(vec.data(), partsize);
    ssize_t sent = ::send(new_fd, vec.data(), partsize, 0);
    if (sent == -1) {
      break;
    }
    filesize -= sent;
  }
  ::close(new_fd);
  std::lock_guard<std::mutex> ifstream_lock(*mu);
  std::cout << "thread " << t_id << " completed" << std::endl;
}

}  // namespace

int main(void) {
  struct addrinfo hints;  // info to fill in
  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // use my IP

  // will point to the results
  int rv;
  struct addrinfo *servinfo_ptr;  // will point to the results
  if ((rv = getaddrinfo(nullptr, kPort, &hints, &servinfo_ptr)) != 0) {
    std::cerr << "svr: getaddrinfo " << gai_strerror(rv) << std::endl;
    return 1;
  }
  // make servinfo a unique ptr
  std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> servinfo(
      servinfo_ptr, freeaddrinfo);
  // listen on sockfd, new conn on new_fd
  int sockfd;
  struct addrinfo *p;

  // loop through all the results and bind to the first we can
  for (p = servinfo.get(); p != nullptr; p = p->ai_next) {
    if((sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
      std::cerr << "svr: socket()" << std::endl;
      ::close(sockfd);
      continue;
  }
  // set options on this socket
    const int yes = 1;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
        -1) {
      std::cerr << "svr: setsockopt()" << std::endl;
      return 1;
    }
// bind this socket
    if(::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
      std::cerr << "svr: bind()" << gai_strerror(rv) << std::endl;
      close(sockfd);
      continue;
    }
    break;
  }

  if (p == nullptr || ::listen(sockfd, kBacklog) == -1) {
    std::cerr << "svr: failed to connect or listen" << strerror(errno)
              << std::endl;
    return 1;
  }

  std::cout << "server waiting for connections..." << std::endl;
  int t_id = 1;
  std::mutex mu;
  //create thread pool and work queue

  while (true) {
    
    const int new_fd = ::accept(sockfd, nullptr, nullptr);
    if (new_fd == -1){
    std::cerr << "svr: error accept()" <<std::endl;
    }
    else{
    std::thread t(std::bind(ftp, new_fd, t_id, &mu));
    t.detach();
    ++t_id;
    }
  }
  ::close(sockfd);
  return 0;
}