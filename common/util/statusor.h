#ifndef STATUSOR
#define STATUSOR

#include <string>
#include <cstddef>

namespace util {

struct Status {
  enum Code { OK, INTERNAL, NOT_FOUND };

  Status(Code code_in, std::string message_in) {
    code = code_in;
    message = std::move(message_in);
  }

  static Status make_OK() { return Status(Code::OK, ""); }

  bool ok() const { return code == OK; }

  Code code;
  std::string message;
};

template <class T>
class StatusOr {
 public:
  StatusOr(T t_) : _ok(true) {
    new (reinterpret_cast<T*>(_buf)) T(std::move(t_));
  }
  StatusOr(Status s_) : _ok(false) {
    new (reinterpret_cast<Status*>(_buf)) Status(std::move(s_));
  }

  ~StatusOr() {
    if (_ok) {
      reinterpret_cast<T*>(_buf)->~T();
    } else {
      reinterpret_cast<Status*>(_buf)->~Status();
    }
  }

  T& operator*() { return *reinterpret_cast<T*>(_buf); }
  T* operator->() { return reinterpret_cast<T*>(_buf); }
  const T& operator*() const { return *reinterpret_cast<T*>(_buf); }
  const T* operator->() const { return reinterpret_cast<T*>(_buf); }
  bool ok() const { return _ok; }
  const Status& error() const { return *reinterpret_cast<Status*>(_buf); }
  Status& error() { return *reinterpret_cast<Status*>(_buf); }

 private:
  const bool _ok;
  std::byte _buf[std::max(sizeof(T), sizeof(Status))];
};
}  // namespace util

namespace std {
ostream& operator<<(ostream& os, const util::Status& status)
{
    os << status.code << ": " << status.message;
    return os;
}

string to_string(const util::Status::Code& code_) {
  switch (code_) {
    case util::Status::Code::OK:
      return "OK";
    case util::Status::Code::INTERNAL:
      return "INTERNAL";
    case util::Status::Code::NOT_FOUND:
      return "NOT_FOUND";
  }
}
}

#endif