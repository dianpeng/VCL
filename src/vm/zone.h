#ifndef ZONE_H_
#define ZONE_H_
#include <vcl/util.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>

namespace vcl {
namespace vm {
namespace zone {

static const size_t kDefaultZoneSize = 512;

// Arena based allocator for working with AST. Currently what we plan to do is
// each file's AST has its own zone allocator then we could dispose each file
// separately. We know VCL has concatenation for function/sub , these
// concatenation
// will not happen on the AST level but use a extra layer called CompilationUnit
// to represent. The CompilationUnit will *not* live on zone, but normal heap
// since this object is transiant. We need compilation only once.
class Zone;
class ZoneObject;
class ZoneString;
template <typename T>
class ZoneVector;

// Zone allocator
class Zone {
 public:
  Zone(size_t capacity = kDefaultZoneSize)
      : m_pool(NULL),
        m_total_segment_size(0),
        m_total_size(0),
        m_capacity(capacity),
        m_initial_capacity(capacity),
        m_size(0),
        m_segment(NULL) {
    CHECK(capacity != 0);
    Grow(capacity, 0);
  }

  // If Zone dies, all memory allocated inside of Zone dies
  ~Zone() { Clear(); }

  // Totally allocated size
  size_t total_size() const { return m_total_size; }

  // Totally allocated segment size
  size_t total_segment_size() const { return m_total_segment_size; }

  // Current pool size
  size_t size() const { return m_size; }

 public:
  template <typename T>
  T Malloc(size_t size) {
    if (m_size < size) {
      Grow(m_capacity * 2, size);
    }
    m_total_size += size;
    return static_cast<T>(Advance(size));
  }

  template <typename T>
  T Realloc(void* old, size_t old_size, size_t new_size) {
    if (old_size >= new_size)
      return static_cast<T>(old);
    else {
      void* new_buf = Malloc<void*>(new_size);
      memcpy(new_buf, old, old_size);
      return static_cast<T>(new_buf);
    }
  }

  void Clear();

 private:
  // Grow the internal pool and guarantee certain size to be free
  // afterh grow , otherwise we have a fatal error
  void Grow(size_t new_cap, size_t guarantee);

  void* Advance(size_t length) {
    void* ret = m_pool;
    m_pool = static_cast<char*>(m_pool) + length;
    m_size -= length;
    return ret;
  }

 private:
  struct Segment;

  // pool's free memory pointer
  void* m_pool;

  // total segment size
  size_t m_total_segment_size;

  // total size
  size_t m_total_size;

  // capacity of the current pool
  size_t m_capacity;

  // initialial capacity of current pool
  const size_t m_initial_capacity;

  // size of the current pool
  size_t m_size;

  // first segment
  Segment* m_segment;
};

// A ZoneObject is kind of object that can only lives on a Zone allocator.
// In VCL, it is the AST nodes. Each file's AST will live on a specific Zone
class ZoneObject {
 public:
  void* operator new(size_t size, Zone* zone) {
    return zone->Malloc<void*>(size);
  }
  void operator delete(void* ptr) {
    VCL_UNUSED(ptr);
    VCL_UNREACHABLE() << "Zone object cannot be deleted!";
  }
  void operator delete(void* ptr, Zone* zone) {
    VCL_UNUSED(ptr);
    VCL_UNUSED(zone);
    VCL_UNREACHABLE() << "Zone object cannot be deleted as array!";
  }
};

// Zone string is immutable and can be used as key for STL conatiner
// It is copyable but not assignable
class ZoneString : public ZoneObject {
 public:
  static const char* kEmptyCStr;
  ZoneString() : m_data(kEmptyCStr), m_size(0) {}

  ZoneString(const ZoneString& rhs) : m_data(rhs.m_data), m_size(rhs.m_size) {}

  ZoneString(Zone* zone, const char* str) : m_data(NULL), m_size(strlen(str)) {
    void* buf = zone->Malloc<void*>(m_size + 1);
    memcpy(buf, str, m_size);
    ((char*)(buf))[m_size] = 0;
    m_data = static_cast<const char*>(buf);
  }

  ZoneString(Zone* zone, const std::string& str)
      : m_data(NULL), m_size(str.size()) {
    void* buf = zone->Malloc<void*>(m_size + 1);
    memcpy(buf, str.c_str(), m_size);
    ((char*)(buf))[m_size] = 0;
    m_data = static_cast<const char*>(buf);
  }

  static ZoneString* New(Zone* zone) {
    return ::new (zone->Malloc<void*>(sizeof(ZoneString))) ZoneString();
  }

  static ZoneString* New(Zone* zone, const char* str) {
    const size_t len = strlen(str);
    void* buf = zone->Malloc<void*>(sizeof(ZoneString) + len + 1);
    return ::new (buf) ZoneString(
        str,
        static_cast<void*>((static_cast<char*>(buf) + sizeof(ZoneString))),
        len);
  }

  static ZoneString* New(Zone* zone, const std::string& str) {
    void* buf = zone->Malloc<void*>(sizeof(ZoneString) + str.size() + 1);
    return ::new (buf) ZoneString(
        str, static_cast<void*>(static_cast<char*>(buf) + sizeof(ZoneString)));
  }

  static ZoneString* New(Zone* zone, const ZoneString& str) {
    return ::new (zone->Malloc<void*>(sizeof(ZoneString))) ZoneString(str);
  }

  ZoneString& operator=(const ZoneString& rhs) {
    if (&rhs == this) return *this;
    m_data = rhs.m_data;
    m_size = rhs.m_size;
    return *this;
  }

 public:
  const char* data() const { return m_data; }

  std::string ToStdString() const { return std::string(m_data, m_size); }

  size_t size() const { return m_size; }

  bool empty() const { return size() == 0; }

 public:  // Interfaces
  char operator[](size_t index) const {
    CHECK(index < m_size);
    return m_data[index];
  }
  bool operator==(const ZoneString& rhs) const {
    if (this == &rhs)
      return true;
    else {
      return strcmp(m_data, rhs.m_data) == 0;
    }
  }
  bool operator==(const char* str) const { return strcmp(m_data, str) == 0; }
  bool operator==(const std::string& rhs) const {
    return strcmp(m_data, rhs.c_str()) == 0;
  }
  bool operator!=(const ZoneString& rhs) const { return !(*this == rhs); }
  bool operator!=(const char* str) const { return !(*this == str); }
  bool operator!=(const std::string& str) const { return !(*this == str); }
  bool operator<(const ZoneString& rhs) const {
    return (strcmp(m_data, rhs.m_data) < 0);
  }
  bool operator<(const char* rhs) const { return (strcmp(m_data, rhs) < 0); }
  bool operator<(const std::string& rhs) const {
    return (strcmp(m_data, rhs.c_str()) < 0);
  }
  bool operator<=(const ZoneString& rhs) const {
    return (strcmp(m_data, rhs.m_data) <= 0);
  }
  bool operator<=(const char* rhs) const { return (strcmp(m_data, rhs) <= 0); }
  bool operator<=(const std::string& rhs) const {
    return (strcmp(m_data, rhs.c_str()) <= 0);
  }
  bool operator>(const ZoneString& rhs) const {
    return (strcmp(m_data, rhs.m_data) > 0);
  }
  bool operator>(const char* rhs) const { return (strcmp(m_data, rhs) > 0); }
  bool operator>(const std::string& rhs) const {
    return (strcmp(m_data, rhs.c_str()) > 0);
  }
  bool operator>=(const ZoneString& rhs) const {
    return (strcmp(m_data, rhs.m_data) >= 0);
  }
  bool operator>=(const char* rhs) const { return (strcmp(m_data, rhs) >= 0); }
  bool operator>=(const std::string& rhs) const {
    return (strcmp(m_data, rhs.c_str()) >= 0);
  }

 private:  // New interfaces used constructor supposed to be little faster
  ZoneString(const char* str, void* buf, size_t len)
      : m_data(static_cast<const char*>(buf)), m_size(len) {
    memcpy(buf, str, m_size);
    ((char*)(m_data))[m_size] = 0;
  }

  ZoneString(const std::string& str, void* buf)
      : m_data(static_cast<const char*>(buf)), m_size(str.size()) {
    memcpy(buf, str.c_str(), m_size);
    ((char*)(m_data))[m_size] = 0;
  }

 private:
  const char* m_data;
  size_t m_size;
};

template <typename T>
class ZoneVector : public ZoneObject {
  // Only certain type of object can be used with ZoneVector
  BOOST_STATIC_ASSERT(boost::is_base_of<ZoneObject, T>::value ||
                      boost::has_trivial_destructor<T>::value);

 public:
  ZoneVector() : m_data(NULL), m_size(0), m_capacity(0) {}

  ZoneVector(Zone* zone, size_t cap)
      : m_data(NULL), m_size(0), m_capacity(cap) {
    m_data = zone->Malloc<T*>(sizeof(T) * cap);
  }

  ZoneVector(Zone* zone, size_t use, size_t cap)
      : m_data(NULL), m_size(use), m_capacity(cap) {
    CHECK(m_size <= m_capacity);
    m_data = zone->Malloc<T*>(sizeof(T) * cap);
    Initialize(0, use);
  }

  static ZoneVector* New(Zone* zone) { return new (zone) ZoneVector(); }

  static ZoneVector* New(Zone* zone, size_t cap) {
    return new (zone) ZoneVector(zone, cap);
  }

  static ZoneVector* New(Zone* zone, size_t use, size_t cap) {
    return new (zone) ZoneVector(zone, use, cap);
  }

 public:  // Public interfaces
  void Add(Zone* zone, const T& val) {
    if (m_capacity == m_size) Reserve(zone, m_size * 2 == 0 ? 2 : m_size * 2);
    m_data[m_size++] = val;
  }
  void Pop() {
    CHECK(m_size > 0);
    --m_size;
  }
  const T& Index(size_t index) const {
    CHECK(index < m_size);
    return m_data[index];
  }
  T& Index(size_t index) {
    CHECK(index < m_size);
    return m_data[index];
  }
  size_t size() const { return m_size; }
  bool empty() const { return size() == 0; }
  size_t capacity() const { return m_capacity; }
  T* data() { return m_data; }
  const T* data() const { return m_data; }
  void Reserve(Zone* zone, size_t cap) {
    if (cap <= m_capacity)
      return;
    else {
      m_data = zone->Realloc<T*>(m_data, sizeof(T) * m_size, cap * sizeof(T));
      m_capacity = cap;
    }
  }
  void Resize(Zone* zone, size_t size) {
    if (m_size == size)
      return;
    else {
      if (m_size < size) {
        Reserve(zone, size);
        Initialize(m_size, size);
      }
      m_size = size;
    }
  }
  const T& operator[](size_t index) const { return Index(index); }
  T& operator[](size_t index) { return Index(index); }

  const T& Last() const {
    CHECK(!empty());
    return Index(size() - 1);
  }

  T& Last() {
    CHECK(!empty());
    return Index(size() - 1);
  }

  const T& First() const {
    CHECK(!empty());
    return Index(0);
  }

  T& First() {
    CHECK(!empty());
    return Index(0);
  }

  void Clear() {
    m_size = 0;  // No destruction of object itself
  }

 private:
  void Initialize(size_t start, size_t end) {
    CHECK(start <= end);
    CHECK(end <= m_capacity);
    for (; start < end; ++start) {
      ::new (start + m_data) T();
    }
  }

 private:
  T* m_data;
  size_t m_size;
  size_t m_capacity;

  VCL_DISALLOW_COPY_AND_ASSIGN(ZoneVector);
};

inline std::ostream& operator<<(std::ostream& output, const ZoneString& str) {
  return output << str.data();
}

}  // namespace zone
}  // namespace vm
}  // namespace vcl

#endif  // ZONE_H_
