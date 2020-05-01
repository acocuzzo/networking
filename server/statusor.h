#ifndef STATUSOR
#define STATUSOR

#include <string>

struct Status
{
  enum Code
  {
    OK,
    INTERNAL,
    NOT_FOUND
  };

  Status(Code code_in, std::string message_in) {
      code = code_in;
      message = std::move(message_in);
  }

  static Status OK()
  {
      return Status(Code::OK, "");
  }

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

  ~StatusOr()
  {
      if (_ok)
      {
          reinterpret_cast<T*>(_buf)->~T();
      }
      else
      {
          reinterpret_cast<Status*>(_buf)->~Status();
      }
  }

  T& operator*() { return *reinterpret_cast<T*>(_buf); }
  T* operator->() { return reinterpret_cast<T*>(_buf); }
  const T& operator*() const { return *reinterpret_cast<T*>(_buf); }
  const T* operator->() const { return reinterpret_cast<T*>(_buf); }
  bool ok() const { return _ok; }
  const Status& error() const { return *reinterpret_cast<Status>(_buf); }
  Status& error() { return *reinterpret_cast<Status>(_buf); }

 private:
  const bool _ok;
  std::byte _buf[std::max(sizeof(T), sizeof(Status))]
};

#endif