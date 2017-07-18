#ifndef VCL_H_
#define VCL_H_
#include <netinet/in.h>

#include <inttypes.h>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <new>
#include <string>
#include <vector>

#include <glog/logging.h>
#include <pcre.h>
#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/type_traits.hpp>
#include <boost/variant.hpp>

#include <vcl/config.h>
#include <vcl/util.h>

namespace vcl {
class Value;
class Object;
class String;
class List;
class Dict;
class ACL;
class Function;
class Extension;
class ExtensionFactory;
class Action;
class Module;
class SubRoutine;
class Iterator;
class DictIterator;
class ListIterator;

template <typename T>
class Handle;

class GC;
class ContextGC;
class ImmutableGC;
class InternalAllocator;
struct ContextOption;
class Context;
struct EngineOption;
class Engine;
class CompiledCode;
class CompiledCodeBuilder;

namespace vm {
namespace ast {
struct File;
}  // namespace ast
class IPPattern;
class Procedure;
class Runtime;
}  // namespace vm

// User defined Malloc and Free hook for context GC object. User could use it
// to do throttling in terms of memory or prevent context using too much memory.
//
// In general VCL doesn't assume memory allocation will fail , so a NULL returns
// back to VCL means *crash* . Sometimes this is desired behavior, sometims this
// is not.
//
// For some cases , user may be want to jump back to C++ code to do some clean
// up
// or kill this context without crashing. So user can set up this hook to
// perform
// jump back. This typically means user needs to throw an exception from the
// hook
// function and then catch it in the outer most code.
//
// Like:
//  throw bad_alloc(OOM);
//
// Then the outer most code catch this exception to know this context is using
// too
// much memory and then kill this context.

class AllocatorHook {
 public:
  virtual void* Malloc(Context*, size_t) = 0;
  virtual void Free(Context*, void* ptr) = 0;
  virtual ~AllocatorHook() {}
};

// Source code information structure, record the file path and content
// of the file for a specific source file
struct SourceCodeInfo {
  std::string source_code;
  std::string file_path;
  SourceCodeInfo() : source_code(), file_path() {}
};

// NOTES: The order *matters*. If you want to add primitive types, please
// append at last in *primitive section* ; if you want to add GCed object,
// please append at last in *GCed object section* .
#define VCL_VALUE_TYPE_LIST(__)                   \
  /* Primtive type object **MUST** starts here */ \
  __(INTEGER, Integer, "integer")                 \
  __(REAL, Real, "real")                          \
  __(NULL, Null, "null")                          \
  __(BOOLEAN, Boolean, "boolean")                 \
  __(SIZE, Size, "size")                          \
  __(DURATION, Duration, "duration")              \
  /* GCed object **MUST** starts here */          \
  __(STRING, String, "string")                    \
  __(ACL, ACL, "acl")                             \
  __(LIST, List, "list")                          \
  __(DICT, Dict, "dict")                          \
  __(FUNCTION, Function, "function")              \
  __(EXTENSION, Extension, "extension")           \
  __(ACTION, Action, "action")                    \
  __(MODULE, Module, "module")                    \
  __(SUB_ROUTINE, SubRoutine, "sub_routine")      \
  __(ITERATOR, Iterator, "iterator")

enum ValueType {
#define __(A, B, C) TYPE_##A,
  VCL_VALUE_TYPE_LIST(__) SIZE_OF_VALUE_TYPE
#undef __  // __
};

inline const char* GetValueTypeName(ValueType val) {
  switch (val) {
#define __(A, B, C) \
  case TYPE_##A:    \
    return C;
    VCL_VALUE_TYPE_LIST(__)
    default:
      return NULL;
#undef __  // __
  }
}

// An object to represent execution status of each customized function.
// We allow several status to be returned from executing a specific customized
// function handler.
// 1. OK  , indicate the function execute correctly and finish with return
// value.
// 2. Fail, indicate the function failed for executing.
// 3. Yield,indicate the function cannot finish and partially executed
// 4. Unimplemented, indicate this function is not implemented by default. We
//    distinguish unimpelemted from fail is sololy for future extension. Because
//    we then could provide *default implementation* for unimplemented method.
// 5. Terminate , indicate this function will be terminated right after calling
// it.
//    This is used to indicate to the user that we are done via a terminate
//    action.
//    VCL script subroutine can generate a terminate via vcl return statement,
//    and
//    also C++ function can generate it by return this as status code.

class MethodStatus VCL_FINAL {
  struct Ok {};
  struct Terminate {};
  struct Fail {
    std::string reason;
  };
  struct Yield {
    boost::any data;
    Yield() : data() {}
    Yield(const boost::any& d) : data(d) {}
  };
  struct Unimplemented {
    std::string description;
  };

  typedef boost::variant<Ok, Terminate, Fail, Yield, Unimplemented> Storage;

 public:
  enum {
    METHOD_OK = 0,
    METHOD_TERMINATE,
    METHOD_FAIL,
    METHOD_YIELD,
    METHOD_UNIMPLEMENTED
  };

  // Static instance for default status code. If user tries to use these
  // instances then user will not have customized field.
  static const MethodStatus kOk;
  static const MethodStatus kTerminate;
  static const MethodStatus kFail;
  static const MethodStatus kYield;
  static const MethodStatus kUnimplemented;

  MethodStatus() : m_storage(Ok()) {}

  MethodStatus(int s) : m_storage() { set_status(s); }

  // Helper function to compose specific type of MethodStatus
  static MethodStatus NewFail(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    return MethodStatus(METHOD_FAIL, format, vl);
  }

  static MethodStatus NewUnimplemented(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    return MethodStatus(METHOD_UNIMPLEMENTED, format, vl);
  }

  static MethodStatus NewYield(const boost::any& data) {
    return MethodStatus(data);
  }

 public:
  int status() const { return m_storage.which(); }

  inline const char* status_name() const;

  // Fail reason and fail status
  const std::string& fail() const {
    DCHECK(m_storage.which() == METHOD_FAIL);
    return boost::get<Fail>(m_storage).reason;
  }

  // Unimplemented and its description
  const std::string& unimplemented() const {
    DCHECK(m_storage.which() == METHOD_UNIMPLEMENTED);
    return boost::get<Unimplemented>(m_storage).description;
  }

  // Yield data
  const boost::any& yield() const {
    DCHECK(m_storage.which() == METHOD_YIELD);
    return boost::get<Yield>(m_storage).data;
  }

  boost::any& yield() {
    DCHECK(m_storage.which() == METHOD_YIELD);
    return boost::get<Yield>(m_storage).data;
  }

  operator bool() const {
    return m_storage.which() == METHOD_OK ||
           m_storage.which() == METHOD_TERMINATE;
  }

  bool is_unimplemented() const {
    return m_storage.which() == METHOD_UNIMPLEMENTED;
  }
  bool is_fail() const { return m_storage.which() == METHOD_FAIL; }
  bool is_ok() const { return m_storage.which() == METHOD_OK; }
  bool is_yield() const { return m_storage.which() == METHOD_YIELD; }
  bool is_terminate() const { return m_storage.which() == METHOD_TERMINATE; }

  void set_unimplemented() { m_storage = Unimplemented(); }
  void set_unimplemented(const char* format, ...);
  void set_fail() { m_storage = Fail(); }
  inline void set_fail(const char* format, ...);
  void set_ok() { m_storage = Ok(); }
  void set_yield() { m_storage = Yield(); }
  void set_yield(const boost::any& value) { m_storage = Yield(value); }
  void set_terminate() { m_storage = Terminate(); }

 private:
  inline void set_status(int s);
  inline MethodStatus(int status, const char* format, va_list vl);
  MethodStatus(const boost::any& data) : m_storage(Yield(data)) {}

  Storage m_storage;
};

// GC root object list and its iterator. Used internally for dynamically
// change global markable objects or reference
struct RootObject {
  size_t ref_count;
  Object* object;

  RootObject() : ref_count(0), object(NULL) {}

  RootObject(Object* obj) : ref_count(1), object(obj) {}
};

// This is a relatively *cheap* implementation for root object tracking. For
// our use cases , a root object needs to be deleted randomly , so we need it
// to be deleted in O(1). Therefore a linked list is a good candidate for us.
// But linked list is not good at traversal and linked list will new/malloc
// whenver a new node is added. This may potentailly fragement and slow down
// our operation since we do have lots of root object needs to be tracked. A
// slightly better implementation will just provide a slab/free-list for node
// but that might not worth it. For now, I just directly use std::list.
typedef std::list<RootObject> RootNodeList;

// The iterator of std::list has a nice feature that delete other node in
// std::lisst
// will not invalidate the iterator. So we just use Iterator as a identity for
// a root object , then whenever a root object wants to remove itself from the
// root object list we just need the iterator and it perform in O(1).
typedef RootNodeList::iterator RootNodeListIterator;

// Handle, a simple RAII wrapper around the AddRoot and RemoveRoot
// functions. User should use this Wrapper to prevent it from being GCed
// when you writing C++ functions.
//
// Example:
// {
//   Handle<String> my_string( gc->NewString("string") , gc );
//   // Do your stuff with my_string
// }
//
// The above code will basically mark the string object you created via
// NewString
// to be reachable , until goes out of the lexical scope. So within that scope,
// user are always able to use that string without GC collecting for that
// string.
//
// To prevent very hard to debug errors like :
//
// my_object->SetProperty(context,*gc->NewString("A"),Value(gc->NewList()));
//
// The above code is very simple but prone to bug, since while evaluating
// gc->NewString,
// the gc->NewList 's return object can be collected because it is unreachable
// during
// that GC cycle. User should *never* write above like code , the safe way to go
// is :
//
// Handle<String> key( gc->NewString("A") , &gc );
// Handle<List> value( gc->NewList() , &gc );
// my_object->SetProperty(context,*key,Value(value.get()));
//
// So the conclusion is, when you write C++ code that is allocating script side
// object
// via GC interfaces, please *ALWAYS* use Handle object.

template <typename T>
class Handle VCL_FINAL {
 public:
  BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
  inline Handle(T*, GC*);
  inline ~Handle();
  inline Handle(const Handle&);
  inline Handle& operator=(const Handle& h);

  T* get() const { return m_value; }
  T& operator*() {
    DCHECK(m_value);
    return *m_value;
  }
  const T& operator*() const {
    DCHECK(m_value);
    return *m_value;
  }
  T* operator->() {
    DCHECK(m_value);
    return m_value;
  }
  const T* operator->() const {
    DCHECK(m_value);
    return m_value;
  }
  operator T*() const { return m_value; }
  inline void Dispose();
  bool empty() const { return m_value == NULL; }

 private:
  T* m_value;
  GC* m_gc;
  RootNodeListIterator m_iterator;
};

// Value represents anything inside of VCL script. It is a holder that can be
// used to store any types of value and also it is the type that is used inside
// of the interpreter's stack. Internally , Value object is represented as
// nan-tagging
// pointer compared to our older version , which directly uses boost::variant.
// This new representation makes our assembly based interepter easier to write
// and
// also the memory footprint is smaller which leads to better cache efficiency.
class Value VCL_FINAL {
 public:
  Value() : m_value(), m_type(TYPE_NULL) {}

  explicit Value(int32_t value) : m_value(value), m_type(TYPE_INTEGER) {}

  explicit Value(double value) : m_value(value), m_type(TYPE_REAL) {}

  explicit Value(bool value) : m_value(value), m_type(TYPE_BOOLEAN) {}

  explicit Value(const vcl::util::Size& value)
      : m_value(value), m_type(TYPE_SIZE) {}

  explicit Value(const vcl::util::Duration& value)
      : m_value(value), m_type(TYPE_DURATION) {}

  inline explicit Value(String* value);
  inline explicit Value(List*);
  inline explicit Value(Dict*);
  inline explicit Value(ACL*);
  inline explicit Value(Function*);
  inline explicit Value(Action*);
  inline explicit Value(Extension*);
  inline explicit Value(Module*);
  inline explicit Value(SubRoutine*);
  inline explicit Value(Iterator*);

  template <typename E>
  inline explicit Value(Handle<E>&);

  template <typename E>
  inline explicit Value(const Handle<E>&);

  Value(const Value& value) : m_value(value.m_value), m_type(value.m_type) {}

  Value& operator=(const Value& value) {
    if (this == &value)
      return *this;
    else {
      m_value = value.m_value;
      m_type = value.m_type;
      return *this;
    }
  }

 public:  // Type
// Type Check IsXXX() function
#define __(A, B, C) \
  bool Is##B() const { return m_type == TYPE_##A; }
  VCL_VALUE_TYPE_LIST(__)
#undef __  // __

  // Getters
  int32_t GetInteger() const {
    DCHECK(IsInteger());
    return boost::get<int32_t>(m_value);
  }
  double GetReal() const {
    DCHECK(IsReal());
    return boost::get<double>(m_value);
  }
  bool GetBoolean() const {
    DCHECK(IsBoolean());
    return boost::get<bool>(m_value);
  }
  const vcl::util::Size& GetSize() const {
    DCHECK(IsSize());
    return boost::get<vcl::util::Size>(m_value);
  }
  const vcl::util::Duration& GetDuration() const {
    DCHECK(IsDuration());
    return boost::get<vcl::util::Duration>(m_value);
  }
  inline String* GetString() const;
  inline ACL* GetACL() const;
  inline List* GetList() const;
  inline Dict* GetDict() const;
  inline Function* GetFunction() const;
  inline Extension* GetExtension() const;
  inline Action* GetAction() const;
  inline Module* GetModule() const;
  inline SubRoutine* GetSubRoutine() const;
  inline Iterator* GetIterator() const;

  // Setters
  void SetInteger(int32_t value) {
    m_value = value;
    m_type = TYPE_INTEGER;
  }
  void SetReal(double value) {
    m_value = value;
    m_type = TYPE_REAL;
  }
  void SetBoolean(bool value) {
    m_value = value;
    m_type = TYPE_BOOLEAN;
  }
  void SetTrue() { SetBoolean(true); }
  void SetFalse() { SetBoolean(false); }
  void SetSize(const vcl::util::Size& value) {
    m_value = value;
    m_type = TYPE_SIZE;
  }
  void SetDuration(const vcl::util::Duration& value) {
    m_value = value;
    m_type = TYPE_DURATION;
  }
  void SetNull() { m_type = TYPE_NULL; }
  inline void SetString(String*);
  inline void SetACL(ACL*);
  inline void SetList(List*);
  inline void SetDict(Dict*);
  inline void SetFunction(Function*);
  inline void SetExtension(Extension*);
  inline void SetAction(Action*);
  inline void SetModule(Module*);
  inline void SetSubRoutine(SubRoutine*);
  inline void SetIterator(Iterator*);

  bool IsObject() const { return m_type > TYPE_DURATION; }

  bool IsPrimitive() const { return !IsObject(); }

  Object* GetObject() const {
    CHECK(IsObject());
    return object();
  }
  ValueType type() const { return m_type; }
  const char* type_name() const { return GetValueTypeName(m_type); }

 public:
  MethodStatus GetProperty(Context*, const String&, Value*) const;
  MethodStatus SetProperty(Context*, const String&, const Value&);

  MethodStatus GetAttribute(Context*, const String&, Value*) const;
  MethodStatus SetAttribute(Context*, const String&, const Value&);

  MethodStatus GetIndex(Context*, const Value& index, Value*) const;
  MethodStatus SetIndex(Context*, const Value& index, const Value&);

  MethodStatus Add(Context*, const Value&, Value*) const;
  MethodStatus Sub(Context*, const Value&, Value*) const;
  MethodStatus Mul(Context*, const Value&, Value*) const;
  MethodStatus Div(Context*, const Value&, Value*) const;
  MethodStatus Mod(Context*, const Value&, Value*) const;

  MethodStatus SelfAdd(Context*, const Value&);
  MethodStatus SelfSub(Context*, const Value&);
  MethodStatus SelfMul(Context*, const Value&);
  MethodStatus SelfDiv(Context*, const Value&);
  MethodStatus SelfMod(Context*, const Value&);

  // NOTES: Please understand the value as input should be the pattern.
  // Typically a match operation obeys following literal:
  // "a string" =~ "a pattern"
  // So left hand side is the string want to be matched and pattern actually
  // appear at right hand side.
  MethodStatus Match(Context*, const Value&, bool*) const;
  MethodStatus NotMatch(Context*, const Value&, bool*) const;
  MethodStatus Unset(Context*);

  MethodStatus Less(Context*, const Value&, bool*) const;
  MethodStatus LessEqual(Context*, const Value&, bool*) const;
  MethodStatus Greater(Context*, const Value&, bool*) const;
  MethodStatus GreaterEqual(Context*, const Value&, bool*) const;
  MethodStatus Equal(Context*, const Value&, bool*) const;
  MethodStatus NotEqual(Context*, const Value&, bool*) const;

  MethodStatus ToString(Context*, std::string*) const;
  MethodStatus ToBoolean(Context*, bool*) const;
  MethodStatus ToInteger(Context*, int32_t*) const;
  MethodStatus ToReal(Context*, double*) const;
  MethodStatus ToDisplay(Context*, std::ostream* output) const;

  MethodStatus NewIterator(Context*, Iterator**);

  // Invoke this value if it is a value that is callable. This function
  // could return yield
  MethodStatus Invoke(Context*, Value*);

  // GC
  inline void Mark();

 public:  // Type value conversion functions
  // The following function will not be called directly from VM/runtime but
  // will be implicitly used for string interpolation. And user is able to
  // use these conversion function via builtin functions. I leave the
  // implementation
  // here simply for convinient purpose to implement string interprolation.
  static bool ConvertToString(Context*, const Value&, String**);
  static bool ConvertToInteger(Context*, const Value&, int32_t*);
  static bool ConvertToReal(Context*, const Value&, double*);
  static bool ConvertToBoolean(Context*, const Value&, bool*);

  // This function is used to cast a size_t value which may be very large
  // and cannot be held in int32_t type to a Value object without losing
  // precision.
  //
  // What it does is it checks the actual value of the size_t value , if it
  // is overflowed in int32_t, then it will store it inside of Value as
  // double type. Otherwise just use int32_t to store it.
  static inline void CastSizeToValueNoLostPrecision(Context*, size_t, Value*);

 private:
  Object* object() const { return boost::get<Object*>(m_value); }

 private:
  boost::variant<Object*, int32_t, double, bool, vcl::util::Duration,
                 vcl::util::Size>
      m_value;
  ValueType m_type;
};

// Specialized for Value objects when we want to Mark a certain Value object
// as root object to prevent it from being collected.
template <>
class Handle<Value> VCL_FINAL {
 public:
  inline Handle(const Value& value, GC* gc);
  inline ~Handle();
  inline Handle(const Handle&);
  inline Handle& operator=(const Handle&);

  Value& get() { return m_value; }
  const Value& get() const { return m_value; }
  Value& operator*() { return m_value; }
  const Value& operator*() const { return m_value; }
  Value* operator->() { return &m_value; }
  const Value* operator->() const { return &m_value; }

 private:
  Value m_value;
  GC* m_gc;
  RootNodeListIterator m_iterator;
};

// An object is something that resides on heap and controlled by GC.
// All object will be derived from Object base class which provides
// basical facilities for handling the GC stuff. Our GC is a simple
// stop the world GC . All managed object will be put on top of a linked
// list *owned* by its Context object.
class Object {
 public:
  virtual ~Object() = 0;

  ValueType type() const { return m_type; }

  const char* type_name() const { return GetValueTypeName(m_type); }

#define __(A, B, C) \
  bool Is##B() const { return m_type == TYPE_##A; }
  VCL_VALUE_TYPE_LIST(__)
#undef __  // __

 public:
  // The following interfaces provide the basic way to extend our type system
  // and make old VCL more flexible. If user just doesn't provide all these
  // functions, then we will simply fallback to default implementation which
  // simply says it doesn't support the corresponding functions.
  //
  // For primitive types and string , some arithmatic operation is already
  // provided.

  // Property Operations
  virtual MethodStatus GetProperty(Context*, const String& key, Value*) const;
  virtual MethodStatus SetProperty(Context*, const String& key,
                                   const Value& value);

  virtual MethodStatus GetAttribute(Context*, const String& key, Value*) const;
  virtual MethodStatus SetAttribute(Context*, const String& key, const Value&);

  virtual MethodStatus GetIndex(Context*, const Value& index, Value*) const;
  virtual MethodStatus SetIndex(Context*, const Value& index, const Value&);

  // Subroutine/Callable Operations
  virtual MethodStatus Invoke(Context*, Value*);

  // Arithmatic Operations
  virtual MethodStatus Add(Context*, const Value&, Value*) const;
  virtual MethodStatus Sub(Context*, const Value&, Value*) const;
  virtual MethodStatus Mul(Context*, const Value&, Value*) const;
  virtual MethodStatus Div(Context*, const Value&, Value*) const;
  virtual MethodStatus Mod(Context*, const Value&, Value*) const;

  virtual MethodStatus SelfAdd(Context*, const Value&);
  virtual MethodStatus SelfSub(Context*, const Value&);
  virtual MethodStatus SelfMul(Context*, const Value&);
  virtual MethodStatus SelfDiv(Context*, const Value&);
  virtual MethodStatus SelfMod(Context*, const Value&);

  virtual MethodStatus Match(Context*, const Value&, bool*) const;
  virtual MethodStatus NotMatch(Context*, const Value&, bool*) const;

  // Unset this object
  virtual MethodStatus Unset(Context*);

  // Comparison
  virtual MethodStatus Less(Context*, const Value&, bool*) const;
  virtual MethodStatus LessEqual(Context*, const Value&, bool*) const;
  virtual MethodStatus Greater(Context*, const Value&, bool*) const;
  virtual MethodStatus GreaterEqual(Context*, const Value&, bool*) const;
  virtual MethodStatus Equal(Context*, const Value&, bool*) const;
  virtual MethodStatus NotEqual(Context*, const Value&, bool*) const;

  // Conversion Operations
  virtual MethodStatus ToString(Context*, std::string*) const;
  virtual MethodStatus ToBoolean(Context*, bool*) const;
  virtual MethodStatus ToInteger(Context*, int32_t*) const;
  virtual MethodStatus ToReal(Context*, double*) const;
  virtual MethodStatus ToDisplay(Context*, std::ostream* output) const;

  // Iterator
  virtual MethodStatus NewIterator(Context*, Iterator**);

  // GC
  void Mark() {
    if (is_white()) {
      set_gray();
      DoMark();
      set_black();
    }
  }

 protected:
  Object(ValueType type) : m_type(type), m_gc_state(GC_WHITE), m_next(NULL) {}

  // This API is used to extend the GC reachability. User that writes extended
  // type needs to overwrite DoMark function to mark its internal data structure
  // and help GC to decide reachablility.
  // Example:
  //
  // class MyFoo : public Extension {
  //  public:
  //   ....
  //
  //   virtual void DoMark();
  //  private:
  //   String* m_internal_string;
  //   Extension* m_other_object;
  // };
  //
  //
  // void MyFoo::DoMark() {
  //   m_internal_string->Mark();
  //   m_other_object->Mark();
  // }
  //
  // Notes: Here when we try to mark String and Extension object, the function
  // we
  // call is not *DoMark* but *Mark*. DoMark is used for user to implement the
  // actual logic but the Mark function is the exact API needs to be called to
  // actually do the real Mark.
  virtual void DoMark() {}

  enum { GC_WHITE, GC_BLACK, GC_GRAY };

  void set_gc_state(int value) { m_gc_state = value; }

  void set_white() { m_gc_state = GC_WHITE; }

  void set_black() { m_gc_state = GC_BLACK; }

  void set_gray() { m_gc_state = GC_GRAY; }

  int gc_state() const { return m_gc_state; }

  bool is_white() const { return m_gc_state == GC_WHITE; }

  bool is_black() const { return m_gc_state == GC_BLACK; }

  bool is_gray() const { return m_gc_state == GC_GRAY; }

 private:
  ValueType m_type;
  uint32_t m_gc_state;
  Object* m_next;

 private:
  friend class Value;
  friend class GC;
  friend class ContextGC;
  friend class ImmutableGC;

  VCL_DISALLOW_COPY_AND_ASSIGN(Object);
};

// A Iterator is a special object that is used to implement iterable concept
// when used in for loop. If an object can provide interface to create its
// internal iterator object then the object can be used in foreach syntax.
class Iterator : public Object {
 protected:
  Iterator() : Object(TYPE_ITERATOR) {}
  virtual ~Iterator() {}

 public:
  // Moving the iterator to the next slots , if the iterator still points
  // to the valid position after the Next operation, it returns true ;
  // otherwise it returns false
  virtual bool Next(Context*) = 0;

  // Test whether the iterator still points to the correct slots of this
  // container.
  virtual bool Has(Context*) const = 0;

  // Derefence the Key/Value from the iterator
  virtual void GetKey(Context*, Value*) const = 0;
  virtual void GetValue(Context*, Value*) const = 0;
};

// A string representation inside of the *script* environment. In C++ side, you
// may still use std::string or whatever string you want to have. This string
// is sololy operated by virtual machine.
class String VCL_FINAL : public Object {
 public:
  const char* data() const { return m_data.c_str(); }

  size_t size() const { return m_data.size(); }

  bool empty() const { return m_data.empty(); }

  const std::string& ToStdString() const { return m_data; }

 public:
  virtual MethodStatus Add(Context*, const Value&, Value*) const;
  virtual MethodStatus SelfAdd(Context*, const Value&);

  virtual MethodStatus Match(Context*, const Value&, bool*) const;
  virtual MethodStatus NotMatch(Context* context, const Value& value,
                                bool* output) const {
    bool result = true;
    MethodStatus ret = !Match(context, value, &result);
    *output = !result;
    return ret;
  }

  virtual MethodStatus Unset(Context* context) {
    VCL_UNUSED(context);
    m_data.clear();
    return MethodStatus::kOk;
  }

  virtual MethodStatus Less(Context*, const Value&, bool*) const;
  virtual MethodStatus LessEqual(Context*, const Value&, bool*) const;
  virtual MethodStatus Greater(Context*, const Value&, bool*) const;
  virtual MethodStatus GreaterEqual(Context*, const Value&, bool*) const;
  virtual MethodStatus Equal(Context*, const Value&, bool*) const;
  virtual MethodStatus NotEqual(Context*, const Value&, bool*) const;

  virtual MethodStatus ToString(Context*, std::string*) const;
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

 public:  // Compatible Interfaces to Ease the use
  bool operator==(const String& rhs) const { return m_data == rhs.m_data; }
  bool operator==(const char* str) const { return m_data == str; }
  bool operator==(const std::string& str) const { return m_data == str; }
  bool operator!=(const String& rhs) const { return !(*this == rhs); }
  bool operator!=(const char* rhs) const { return !(*this == rhs); }
  bool operator!=(const std::string& rhs) const { return !(*this == rhs); }
  bool operator<(const String& rhs) const { return m_data < rhs.m_data; }
  bool operator<(const char* rhs) const { return m_data < rhs; }
  bool operator<(const std::string& rhs) const { return m_data < rhs; }
  bool operator<=(const String& rhs) const { return m_data <= rhs.m_data; }
  bool operator<=(const char* rhs) const { return m_data <= rhs; }
  bool operator<=(const std::string& rhs) const { return m_data <= rhs; }
  bool operator>(const String& rhs) const { return m_data > rhs.m_data; }
  bool operator>(const char* rhs) const { return m_data > rhs; }
  bool operator>(const std::string& rhs) const { return m_data > rhs; }
  bool operator>=(const String& rhs) const { return m_data >= rhs.m_data; }
  bool operator>=(const char* rhs) const { return m_data >= rhs; }
  bool operator>=(const std::string& rhs) const { return m_data >= rhs; }

 private:
  String(const char* str) : Object(TYPE_STRING), m_data(str), m_regex() {}

  String(const std::string& str)
      : Object(TYPE_STRING), m_data(str), m_regex() {}

  virtual ~String() {}

  MethodStatus Match(Context* context, const String& string, bool* output) {
    return m_regex.Match(context, m_data, string, output);
  }

 private:
  std::string m_data;

  // Regex operations
  class Regex {
   public:
    Regex() : ctx(NULL), extra(NULL) {}
    MethodStatus Match(Context*, const String&, const String&, bool*);
    ~Regex() {
      if (ctx) pcre_free(ctx);
      if (extra) pcre_free(extra);
    }

   private:
    MethodStatus Init(Context*, const String& pattern);
    pcre* ctx;
    pcre_extra* extra;

   private:
    VCL_DISALLOW_COPY_AND_ASSIGN(Regex);
  };
  Regex m_regex;

  friend class ImmutableGC;
  friend class GC;
  VCL_DISALLOW_COPY_AND_ASSIGN(String);
};

namespace detail {

struct DefaultStringHasher {
  uint32_t operator()(const char* string, size_t length) const {
    uint32_t ret = 17771;
    size_t i;
    for (i = 0; i < length; ++i) {
      ret = (ret ^ ((ret << 5) + (ret >> 2))) + (uint32_t)(string[i]);
    }
    return ret;
  }
};

}  // namespace detail

// StringDict
// StringDict is a *customized* hash table. This StringDict doesn't use
// stl/boost unordered_map internally.
// The hash table here uses open addressing mechanism. Open addressing has
// several advantage in typical script language evnrionment.
// 1. In most cases, the hash table won't grow to many entries but tend to
//    have lots of small hash table with few entries. So it is important to
//    make hash function memory efficient. stl::unordered_map uses prime number
//    table size and chain resolution.
// 2. Deletion is not very frequent in general. Prefering open addressing hash
//    table makes read and search operation relative fast and efficient.
template <typename T, typename Hasher = detail::DefaultStringHasher>
class StringDict VCL_FINAL {
 public:
  typedef std::pair<const String*, T> ValueType;

 private:
  struct HashEntry {
    ValueType pair;
    uint32_t full_hash;
    uint32_t next : 29;
    uint32_t more : 1;
    uint32_t used : 1;
    uint32_t del : 1;

    bool Equal(const char* string, uint32_t hash) const {
      return full_hash == hash && *pair.first == string;
    }

    HashEntry() : pair(), full_hash(0), next(0), more(0), used(0), del(0) {}
  };

  static uint32_t HashKey(const char* string, size_t length) {
    return Hasher()(string, length);
  }

  typedef std::vector<HashEntry> HashVector;

  template <typename Iter, typename VT>
  class IteratorImpl {
   public:
    typedef VT ValueType;
    IteratorImpl(const Iter& iter, const Iter& end)
        : m_iterator(iter), m_end(end) {
      NextAvailable();
    }

    IteratorImpl() : m_iterator(), m_end() {}

    IteratorImpl(const IteratorImpl& that)
        : m_iterator(that.m_iterator), m_end(that.m_end) {}

    IteratorImpl& operator=(const IteratorImpl& that) {
      if (this != &that) {
        m_iterator = that.m_iterator;
        m_end = that.m_end;
      }
      return *this;
    }

    bool operator==(const IteratorImpl& that) const {
      DCHECK(m_end == that.m_end);
      return m_iterator == that.m_iterator;
    }

    bool operator!=(const IteratorImpl& that) const {
      DCHECK(m_end == that.m_end);
      return m_iterator != that.m_iterator;
    }

    IteratorImpl operator++(int) {
      DCHECK(m_iterator != m_end);
      IteratorImpl v(*this);
      ++m_iterator;
      NextAvailable();
      return v;
    }

    IteratorImpl& operator++() {
      DCHECK(m_iterator != m_end);
      ++m_iterator;
      NextAvailable();
      return *this;
    }

    ValueType& operator*() {
      DCHECK(m_iterator != m_end);
      return m_iterator->pair;
    }

    ValueType* operator->() {
      DCHECK(m_iterator != m_end);
      return &(m_iterator->pair);
    }

   private:
    void NextAvailable() {
      while (m_iterator != m_end) {
        typename Iter::reference v = *m_iterator;
        if (v.used && !v.del) break;
        ++m_iterator;
      }
    }

    Iter m_iterator;
    Iter m_end;
  };

  static const size_t kDefaultCapSize = 4;

 public:
  StringDict(size_t cap = kDefaultCapSize) : m_entry(), m_used(0), m_size(0) {
    DCHECK(!(cap & (cap - 1)));
    m_entry.resize(cap);
  }

  StringDict(const StringDict& sd)
      : m_entry(sd.m_entry), m_used(sd.m_used), m_size(sd.m_size) {}

  StringDict& operator=(const StringDict& sd) {
    if (this != &sd) {
      StringDict temp(sd);
      Swap(&temp);
    }
    return *this;
  }

  void Swap(StringDict* that) {
    std::swap(m_used, that->m_used);
    std::swap(m_size, that->m_size);
    m_entry.swap(that->m_entry);
  }

  bool empty() const { return size() == 0; }

  size_t size() const { return m_size; }

  size_t used() const { return m_used; }

  size_t capacity() const { return m_entry.size(); }

  bool Insert(const String&, const T&);

  template <typename ALLOC>
  bool Insert(ALLOC*, const char*, const T&);

  template <typename ALLOC>
  bool Insert(ALLOC*, const std::string&, const T&);

  bool Update(const String&, const T&);

  template <typename ALLOC>
  bool Update(ALLOC*, const char*, const T&);

  template <typename ALLOC>
  bool Update(ALLOC* gc, const std::string& key, const T& value) {
    return Update(gc, key.c_str(), value);
  }

  void InsertOrUpdate(const String&, const T&);

  template <typename ALLOC>
  void InsertOrUpdate(ALLOC*, const char*, const T&);

  template <typename ALLOC>
  void InsertOrUpdate(ALLOC* gc, const std::string& key, const T& value) {
    InsertOrUpdate(gc, key.c_str(), value);
  }

  T* Find(const String& key) const;
  T* Find(const char* key) const;
  T* Find(const std::string& key) const { return Find(key.c_str()); }

  bool Remove(const String&, T* output);
  bool Remove(const char*, T* output);
  bool Remove(const std::string& key, T* output) {
    return Remove(key.c_str(), output);
  }

  void Clear();

 private:
  enum { OPT_FIND, OPT_INSERT };

  template <int OPTION>
  HashEntry* FindEntry(const char*, uint32_t) const;

  void Rehash();

 public:  // GC
  static void DoGCMark(StringDict<T, Hasher>*);
  static void DoDelete(StringDict<T, Hasher>*);

  typedef IteratorImpl<typename HashVector::iterator, ValueType> Iterator;

  typedef IteratorImpl<typename HashVector::const_iterator, const ValueType>
      ConstIterator;

  Iterator Begin() { return Iterator(m_entry.begin(), m_entry.end()); }
  Iterator End() { return Iterator(m_entry.end(), m_entry.end()); }
  ConstIterator Begin() const {
    const HashVector& hv = m_entry;
    return ConstIterator(hv.begin(), hv.end());
  }
  ConstIterator End() const {
    const HashVector& hv = m_entry;
    return ConstIterator(hv.end(), hv.end());
  }

 private:
  mutable HashVector m_entry;
  uint32_t m_used;  // How many slots are occupied, used to decide whether we
                    // need to do a rehash
  uint32_t m_size;  // How many slots are actually used
};

class List VCL_FINAL : public Object {
 public:
  static const size_t kMaximumListSize =
#ifdef VCL_MAXIMUM_LIST_SIZE
      VCL_MAXIMUM_LIST_SIZE
#else
      1024 * 4 * 64
#endif  // VCL_MAXIMUM_LIST_SIZE
      ;

  BOOST_STATIC_ASSERT(kMaximumListSize < std::numeric_limits<int32_t>::max());

  virtual MethodStatus GetIndex(Context*, const Value&, Value*) const;
  virtual MethodStatus SetIndex(Context*, const Value&, const Value&);

  virtual MethodStatus Unset(Context* context) {
    VCL_UNUSED(context);
    m_list.clear();
    return MethodStatus::kOk;
  }
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;
  virtual MethodStatus NewIterator(Context*, Iterator**);

 public:  // List interfaces
  void Push(const Value& value) { m_list.push_back(value); }
  void Pop() { m_list.pop_back(); }
  void Reserve(size_t cap) { m_list.reserve(cap); }
  void Resize(size_t cap) { m_list.resize(cap); }
  void Clear() { m_list.clear(); }
  size_t size() const { return m_list.size(); }
  bool empty() const { return m_list.empty(); }
  Value& operator[](size_t idx) { return m_list[idx]; }
  const Value& operator[](size_t idx) const { return m_list[idx]; }
  Value& Index(size_t idx) { return m_list[idx]; }
  const Value& Index(size_t idx) const { return m_list[idx]; }

 private:
  virtual void DoMark();

 private:
  List() : Object(TYPE_LIST), m_list() {}

  List(size_t cap) : Object(TYPE_LIST), m_list() { m_list.reserve(cap); }

  typedef std::vector<Value> ListType;
  ListType m_list;

  friend class GC;
  friend class ListIterator;
  VCL_DISALLOW_COPY_AND_ASSIGN(List);
};

class ListIterator : public Iterator {
 public:
  ListIterator(List* list) : m_list(list), m_itr(list->m_list.begin()) {}

  // Mark the list if the iterator existed
  virtual void DoMark() { m_list->Mark(); }

  // Iterator public interfaces
  virtual bool Has(Context* context) const {
    VCL_UNUSED(context);
    return m_itr != m_list->m_list.end();
  }

  virtual bool Next(Context* context) {
    VCL_UNUSED(context);
    ++m_itr;
    return m_itr != m_list->m_list.end();
  }

  virtual void GetKey(Context* context, Value* value) const {
    VCL_UNUSED(context);
    value->SetInteger(std::distance(m_list->m_list.begin(), m_itr));
  }

  virtual void GetValue(Context* context, Value* value) const {
    VCL_UNUSED(context);
    *value = *m_itr;
  }

 private:
  List* m_list;
  typedef List::ListType::iterator Iterator;
  mutable Iterator m_itr;
  VCL_DISALLOW_COPY_AND_ASSIGN(ListIterator);
};

// Dict represents a dictionary type inside of the VCL script environment
class Dict VCL_FINAL : public Object {
  typedef StringDict<Value> DictType;

 public:
  static const size_t kMaximumDictSize =
#ifdef VCL_MAXIMUM_DICT_SIZE
      VCL_MAXIMUM_DICT_SIZE
#else
      1024 * 4 * 64
#endif  // VCL_MAXIMUM_DICT_SIZE
      ;

  BOOST_STATIC_ASSERT(kMaximumDictSize < std::numeric_limits<int32_t>::max());

  // Property
  virtual MethodStatus GetProperty(Context*, const String&, Value*) const;
  virtual MethodStatus SetProperty(Context*, const String&, const Value&);

  // Index
  virtual MethodStatus GetIndex(Context*, const Value&, Value*) const;
  virtual MethodStatus SetIndex(Context*, const Value&, const Value&);

  // Attribute
  virtual MethodStatus GetAttribute(Context* context, const String& key,
                                    Value* output) const {
    return GetProperty(context, key, output);
  }
  virtual MethodStatus SetAttribute(Context* context, const String& key,
                                    const Value& value) {
    return SetProperty(context, key, value);
  }

  virtual MethodStatus Unset(Context* context) {
    VCL_UNUSED(context);
    m_dict.Clear();
    return MethodStatus::kOk;
  }

  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

  virtual MethodStatus NewIterator(Context*, ::vcl::Iterator**);

 public:  // Delegated APIs
  bool Insert(const String& key, const Value& v) {
    return m_dict.Insert(key, v);
  }
  void InsertOrUpdate(const String& key, const Value& v) {
    m_dict.InsertOrUpdate(key, v);
  }
  bool Find(const String& key, Value* output) const {
    Value* result = m_dict.Find(key);
    if (!result) {
      return false;
    } else {
      *output = *result;
      return true;
    }
  }
  bool Remove(const String& key, Value* output) {
    return m_dict.Remove(key, output);
  }
  void Clear() { m_dict.Clear(); }

  size_t size() const { return m_dict.size(); }
  bool empty() const { return m_dict.empty(); }

 public:
  // For unittest purpose
  bool Find(const char* key, Value* output) const {
    Value* result = m_dict.Find(key);
    if (!result) {
      return false;
    } else {
      *output = *result;
      return true;
    }
  }
  bool Find(const std::string& key, Value* output) const {
    return Find(key.c_str(), output);
  }

 public:
  typedef DictType::Iterator Iterator;
  typedef DictType::ConstIterator ConstIterator;
  Iterator Begin() { return m_dict.Begin(); }
  Iterator End() { return m_dict.End(); }
  ConstIterator Begin() const { return m_dict.Begin(); }
  ConstIterator End() const { return m_dict.End(); }

 private:
  virtual void DoMark() { DictType::DoGCMark(&m_dict); }

 private:
  Dict(size_t reserve) : Object(TYPE_DICT), m_dict(reserve) {}

  Dict() : Object(TYPE_DICT), m_dict() {}

  DictType m_dict;

  friend class GC;
  friend class DictIterator;
  VCL_DISALLOW_COPY_AND_ASSIGN(Dict);
};

class DictIterator : public Iterator {
 public:
  DictIterator(Dict* dict) : m_dict(dict), m_itr(dict->Begin()) {}

  virtual void DoMark() { m_dict->Mark(); }

  virtual bool Has(Context* context) const {
    VCL_UNUSED(context);
    return m_itr != m_dict->End();
  }

  virtual bool Next(Context* context) {
    VCL_UNUSED(context);
    ++m_itr;
    return m_itr != m_dict->End();
  }

  virtual void GetKey(Context* context, Value* value) const {
    VCL_UNUSED(context);
    value->SetString(const_cast<String*>(m_itr->first));
  }

  virtual void GetValue(Context* context, Value* value) const {
    VCL_UNUSED(context);
    *value = m_itr->second;
  }

 private:
  Dict* m_dict;
  mutable Dict::Iterator m_itr;

  VCL_DISALLOW_COPY_AND_ASSIGN(DictIterator);
};

// Varnish ACL pattern lists. The Varinish ACL forms a special pattern
// or grammar. Inside of ACL scope, I will simply treat all the strings
// as ip address patterns. ACL as a whole forms a value and it can be
// used to compare against with.
class ACL VCL_FINAL : public Object {
 public:
  virtual MethodStatus Match(Context*, const Value&, Value*) const;
  virtual MethodStatus Unset(Context* context) {
    VCL_UNUSED(context);
    return MethodStatus::NewUnimplemented("Unset not implemented for type ACL");
  }
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

  bool Match(const in_addr&) const;
  bool Match(const in6_addr&) const;
  bool Match(const char*) const;

  virtual ~ACL();

 private:
  ACL(vm::IPPattern* pattern);
  boost::scoped_ptr<vm::IPPattern> m_impl;

  friend class GC;
  friend class ACLBuilder;
  VCL_DISALLOW_COPY_AND_ASSIGN(ACL);
};

// This class provides facilities for user to compose a ACL list in C++
class ACLBuilder {
 public:
  ACLBuilder(ContextGC*);
  // Add a specific ip-address pattern into the ACL
  bool AddPattern(const std::string& ip_address, bool negative);
  // Add a classical varnish VCL style address pattern into ACL
  bool AddAddress(const std::string& ip_address, bool negative,
                  uint32_t mask = 0);
  bool AddAddress(const in_addr&, bool negative, uint32_t mask = 0);
  bool AddAddress(const in6_addr&, bool negative, uint32_t mask = 0);
  // After creating the ACL list by using AddXXX api, user needs to
  // release object from ACLBuilder otherwise it is deleted.
  ACL* Release() { return m_acl.release(); }

 private:
  vcl::util::scoped_ptr<ACL> m_acl;
};

// Function is just an object that user can extend to register C++ function
// into the VCL environment to use. Personally I want user to organize their
// functions into *module* for clarity. But any single function that needs to
// be exposed into VCL environment must be derived from the Function base class.
class Function : public Object {
 public:
  // Default implementation
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

  const std::string& name() const { return m_name; }

 protected:
  // Prototype string needs to be provided for the function
  // and it will be compiled into a function prototype objects
  // later on for checking the input argument
  Function(const std::string& name) : Object(TYPE_FUNCTION), m_name(name) {}

  std::string m_name;

  friend class GC;
  VCL_DISALLOW_COPY_AND_ASSIGN(Function);
};

// Extension is used to define compound type , well, if user defined it .
// If user doesn't define the extension compound type, then the
// virtual machine will tell an error about the type user tries to register.
//
// The reason user want to have Extension by customization is that it allows
// user to store extern data inside of the Extension object. Those data is
// opaque to the virtual machine but needed to carry semantic option for
// host application.
// It is a purposely lefted type loophole for user to extend the runtime
class Extension : public Object {
 public:
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;
  const std::string& extension_name() const { return m_extension_name; }

 protected:
  // Inheritance Only
  Extension(const std::string& ext_name)
      : Object(TYPE_EXTENSION), m_extension_name(ext_name) {}

 private:
  // Extensionure definitions
  std::string m_extension_name;

  friend class DefaultExtension;
  friend class GC;
  VCL_DISALLOW_COPY_AND_ASSIGN(Extension);
};

// Extension Factory
// Extension factory are accossiated factory class for certain type of
// extension.
// When virtual machine starts to execute the code , if there is a extension,
// the virtual machine will use type name to lookup the factor and use it to
// create a extension objects. The extension factory is mostly used to iosolate
// the user's customized type from the managed environment.
class ExtensionFactory {
 public:
  // Only interfaces that factory class needs to provide
  virtual Extension* NewExtension(Context*) = 0;
  virtual ~ExtensionFactory() {}
};

// This value type is used to handle sick varnish return statements.
// The return statement returns something so freaking wired. It doesn't
// have normal types ! In its transpiler it really just hack around it.
// But in our version we cannot bypass the value system since we are a
// real script envrionment. I setup this Action type sololy to meet the
// varnish freaking return statements.
//
// However inside of its freaking return statement, it allows function call
// in some excetional cases. How I wish the author learn some simple computer
// linguistic lessons ...
// To meet its sick requirements, those function call will not show up here
// but really will register as global function. The return value of those
// function will become the value of sub's return in VCL.
//
// Currently, those 3 sick cases are :
// 1. synth
// 2. pass
// 3. vcl

#define VCL_ACTION_LIST(__)  \
  __(ACT_OK, "ok")           \
  __(ACT_FAIL, "fail")       \
  __(ACT_PIPE, "pipe")       \
  __(ACT_HASH, "hash")       \
  __(ACT_PURGE, "purge")     \
  __(ACT_LOOKUP, "lookup")   \
  __(ACT_RESTART, "restart") \
  __(ACT_FETCH, "fetch")     \
  __(ACT_MISS, "miss")       \
  __(ACT_DELIVER, "deliver") \
  __(ACT_RETRY, "retry")     \
  __(ACT_ABANDON, "abandon") \
  /* Fallback action type */ \
  __(ACT_EXTENSION, "extension")

enum ActionType {
#define __(A, B) A,
  VCL_ACTION_LIST(__) SIZE_OF_ACTION_TYPE
#undef __  // __
};

class Action VCL_FINAL : public Object {
 public:
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

 public:
  int action_code() const { return m_action_code; }
  const char* action_code_name() const;

 private:
  Action(ActionType code)
      : Object(TYPE_ACTION), m_action_code(code), m_extension_name() {
    CHECK(m_action_code != ACT_EXTENSION);
  }

  ActionType m_action_code;
  std::string m_extension_name;
  friend class GC;
  VCL_DISALLOW_COPY_AND_ASSIGN(Action);
};

// Module is just a collection of object functions other stuff that user
// can import by using the import keyword.
class Module VCL_FINAL : public Object {
  typedef StringDict<Value> Map;

 public:
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

  // Module is read only so only GetProperty is defined here
  virtual MethodStatus GetProperty(Context*, const String&, Value*) const;

  // Internal APIs for user to compose a module
  void AddProperty(const String& key, const Value& value) {
    m_map.InsertOrUpdate(key, value);
  }

  bool FindProperty(const String& key, Value* output) const {
    const Value* result = m_map.Find(key);
    if (result) {
      *output = *result;
      return true;
    } else {
      return false;
    }
  }

  bool RemoveProperty(const String& key) { return m_map.Remove(key, NULL); }

  void ClearProperty() { m_map.Clear(); }

  virtual void DoMark() { Map::DoGCMark(&m_map); }

  const std::string& name() const { return m_name; }

 private:
  Module(const std::string& name)
      : Object(TYPE_MODULE), m_map(), m_name(name) {}

  Map m_map;
  std::string m_name;

  friend class GC;
  VCL_DISALLOW_COPY_AND_ASSIGN(Module);
};

// A SubRoutine is a defined script side routine/function in script. User is
// allowed
// to invoke it basically for customization reason. All SubRoutine are GCed
// objects
// but it stores inside its global context so unless a Context is destroyed then
// the SubRoutine will *not* be GCed.
//
// The only defined operation on SubRoutine is Invoke function call which user
// is allowed to use it call a script side defined function and perform
// customized
// logic afterwards.
class SubRoutine VCL_FINAL : public Object {
 public:
  virtual ~SubRoutine();
  virtual MethodStatus ToDisplay(Context*, std::ostream*) const;

 public:
  const std::string& name() const;
  const std::string& protocol() const;
  size_t argument_size() const;

 public:  // Opaque pointer to internal state. User is not expected to use
          // it or understand it.
  vm::Procedure* procedure() const { return m_procedure; }

 private:
  SubRoutine(vm::Procedure*);
  vm::Procedure* m_procedure;
  friend class GC;
  friend class InternalAllocator;
  VCL_DISALLOW_COPY_AND_ASSIGN(SubRoutine);
};

// A stop the world mark and trace GC. User should mostly never have GC kicks in
// since we don't have loop at all, the time GC kicks in should be extreamly
// rare and also the GC should be mostly fast.
class GC {
 protected:
  virtual ~GC();

  static const size_t kMinimumGCGap =
#ifdef VCL_MINIMUM_GC_GAP
      VCL_MINIMUM_GC_GAP
#else
      5000
#endif  // VCL_MINIMUM_GC_GAP
      ;

  GC(size_t next_gc_trigger, double gc_ratio,
     const size_t minimum_gc_gap = kMinimumGCGap)
      : m_gc_start(NULL),
        m_gc_size(0),
        m_next_gc(next_gc_trigger),
        m_gc_ratio(gc_ratio),
        m_minimum_gc_gap(minimum_gc_gap),
        m_gc_times(0) {
    DCHECK(gc_ratio >= 0.0 && gc_ratio <= 1.0);
    m_next_gc = m_next_gc < minimum_gc_gap ? minimum_gc_gap : m_next_gc;
  }

  // Try to do the GC collection based on the internal trigger mechanism
  // if a GC process is triggered , it returns true ; otherwise returns false
  bool TryCollect() {
    if (CanCollect()) {
      ForceCollect();
      return true;
    }
    return false;
  }

  void ForceCollect() {
    ++m_gc_times;
    Mark();
    Collect();
  }

  // Force to do a GC collect operation. It will return how many objects have
  // been collected. Be cautious this process is not very fast
  size_t Collect();

  // Mark all the managed object
  virtual void Mark() = 0;

  size_t gc_size() const { return m_gc_size; }

  size_t next_gc_trigger() const { return m_next_gc; }

  double gc_ratio() const { return m_gc_ratio; }

  void set_gc_ratio(double ratio) {
    DCHECK(ratio >= 0 && ratio <= 1.0);
    m_gc_ratio = ratio;
  }

  void set_next_gc_trigger(size_t gc_trigger) { m_next_gc = gc_trigger; }

 protected:
  // Virtual function for customization of these malloc stuff. User could
  // then write a slab based Malloc and Free to even minimize the GC cost
  virtual void* Malloc(size_t length) { return ::malloc(length); }
  virtual void Free(void* ptr) { ::free(ptr); }
  template <typename T>
  void Delete(T* ptr) {
    vcl::util::Destruct(ptr);
    Free(static_cast<void*>(ptr));
  }

 public:  // General purpose object that user could use to allocate object
          // from managed heap. The following APIs will *not* trigger GC
          // procedure so it is mostly safe for anybody to use. However since
          // they don't trigger GC, in theory those API may run out of memory.
  // User can manually trigger GC by calling TryCollect function. In most
  // cases, user is not supposed to use the following APIs. The only use
  // case here is to use these APIs to create customized object and
  // register them into the context and environment before running the
  // script.
  template <typename T>
  T* New() {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T());
  }

  // If we have C++ 11 as default, then we could migrate to use variadic
  // template argument here. But for now, I will just use C++03 standard
  template <typename T, typename A1>
  T* New(const A1& a1) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T(a1));
  }

  template <typename T, typename A1, typename A2>
  T* New(const A1& a1, const A2& a2) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T(a1, a2));
  }

  template <typename T, typename A1, typename A2, typename A3>
  T* New(const A1& a1, const A2& a2, const A3& a3) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T(a1, a2, a3));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4>
  T* New(const A1& a1, const A2& a2, const A3& a3, const A4& a4) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T(a1, a2, a3, a4));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4,
            typename A5>
  T* New(const A1& a1, const A2& a2, const A3& a3, const A4& a4, const A5& a5) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T(a1, a2, a3, a4, a5));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4,
            typename A5, typename A6>
  T* New(const A1& a1, const A2& a2, const A3& a3, const A4& a4, const A5& a5,
         const A6& a6) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T))) T(a1, a2, a3, a4, a5, a6));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4,
            typename A5, typename A6, typename A7>
  T* New(const A1& a1, const A2& a2, const A3& a3, const A4& a4, const A5& a5,
         const A6& a6, const A7& a7) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T)))
                             T(a1, a2, a3, a4, a5, a6, a7));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4,
            typename A5, typename A6, typename A7, typename A8>
  T* New(const A1& a1, const A2& a2, const A3& a3, const A4& a4, const A5& a5,
         const A6& a6, const A7& a7, const A8& a8) {
    BOOST_STATIC_ASSERT(boost::is_base_of<Object, T>::value);
    return LinkObject<T>(::new (Malloc(sizeof(T)))
                             T(a1, a2, a3, a4, a5, a6, a7, a8));
  }

 public:
  void Dump(std::ostream*) const;

 public:
  virtual RootNodeListIterator AddRoot(Object* object) {
    VCL_UNUSED(object);
    return RootNodeListIterator();
  }

  virtual void RemoveRoot(RootNodeListIterator iterator) {
    VCL_UNUSED(iterator);
  }

 public:
  size_t gc_times() const { return m_gc_times; }

 private:
  template <typename T>
  T* LinkObject(Object* object) {
    object->m_next = m_gc_start;
    m_gc_start = object;
    ++m_gc_size;
    return static_cast<T*>(object);
  }

  bool CanCollect() const { return m_gc_size >= m_next_gc; }

  void Recalculate(size_t collected) {
    if (m_gc_size) {
      // This much has been collecte
      double ratio =
          static_cast<double>(collected) / static_cast<double>(m_gc_size);

      double left = m_gc_ratio - ratio;

      // Next GC trigger time size
      int64_t next_gc = static_cast<int64_t>(m_next_gc);

      next_gc = next_gc * (1.0 + left);

      m_next_gc = static_cast<size_t>(next_gc);

      const size_t next_gc_size = m_gc_size - collected;

      if (m_next_gc - next_gc_size < m_minimum_gc_gap)
        m_next_gc = next_gc_size + m_minimum_gc_gap;
    } else {
      DCHECK(collected == 0);
    }
  }

  // The start pointer of the GC managed chain
  Object* m_gc_start;

  // Current GC chain size
  size_t m_gc_size;

  // Next GC size which will be used to trigger a collect
  size_t m_next_gc;

  // The ratio used to decide next GC trigger size
  double m_gc_ratio;

  // GC gap
  size_t m_minimum_gc_gap;

  // GC trigger times
  size_t m_gc_times;

  friend class Context;
  friend class Engine;
  friend class InternalAllocator;
};

// A specific GC that is sololy used by *Context* object in an isolated
// execution
// environment. Each context will have its *own* GC. The engine object will not
// use ContextGC but ImmutableGC.
class ContextGC VCL_FINAL : public GC {
 public:
  ContextGC(size_t trigger, double ratio, Context* ctx)
      : GC(trigger, ratio), m_root_list(), m_context(ctx), m_hook() {}

  virtual ~ContextGC() {}

  // Make those APIs to public users
  using GC::TryCollect;
  using GC::ForceCollect;

  using GC::gc_size;
  using GC::next_gc_trigger;
  using GC::gc_ratio;
  using GC::set_gc_ratio;
  using GC::set_next_gc_trigger;

 public:
  // String
  String* NewString(const char* str) {
    TryCollect();
    return New<String>(str);
  }
  String* NewString(const std::string& str) { return NewString(str.c_str()); }

  // List
  List* NewList(size_t reserve) {
    TryCollect();
    return New<List>(reserve);
  }
  List* NewList() {
    TryCollect();
    return New<List>();
  }

  // Dict
  Dict* NewDict(size_t reserve) {
    TryCollect();
    return New<Dict>(reserve);
  }

  Dict* NewDict() {
    TryCollect();
    return New<Dict>();
  }

  // Action
  Action* NewAction(ActionType act_code) {
    TryCollect();
    return New<Action>(act_code);
  }

  // Module
  Module* NewModule(const std::string& name) {
    TryCollect();
    return New<Module>(name);
  }

 public:  // Root node management. If an object is added to the root
          // node then it will be prevented from being garbage collected

  // Add a Object into the root node list and returns you an iterator
  // later on you could use it to *remove* this object from root node
  // If user Add an object that's already existed inside of the root_list,
  // behavior is undefined. It will result in memory leak
  virtual RootNodeListIterator AddRoot(Object* object) {
    return m_root_list.insert(m_root_list.end(), RootObject(object));
  }

  // Remove an object from root node list indicated by the input iterator.
  // Most use case should be captured by using Handle instead of
  // directly using AddRoot and RemoveRoot functions
  virtual void RemoveRoot(RootNodeListIterator iterator) {
    DCHECK(iterator->ref_count > 0);
    --iterator->ref_count;
    if (iterator->ref_count == 0) m_root_list.erase(iterator);
  }

 public:  // GC Hook for memory sandbox
  void SetAllocatorHook(AllocatorHook* hook) { m_hook.reset(hook); }

  AllocatorHook* GetAllocatorHook() const { return m_hook.get(); }

 private:
  virtual void* Malloc(size_t length) {
    if (m_hook) {
      return m_hook->Malloc(m_context, length);
    } else {
      return GC::Malloc(length);
    }
  }

  virtual void Free(void* ptr) {
    if (m_hook) {
      return m_hook->Free(m_context, ptr);
    } else {
      return GC::Free(ptr);
    }
  }

 private:
  inline String* NewStringImpl(const char* str);

 private:
  virtual void Mark();
  RootNodeList m_root_list;
  Context* m_context;
  boost::scoped_ptr<AllocatorHook> m_hook;

  friend class Context;
  VCL_DISALLOW_COPY_AND_ASSIGN(ContextGC);
};

// ImmutableGC is a bump allocator that traces all the allocated resources.
// It will not have any GC cycle. The gc trigger is not exposed here.
// And also for engine level Value, the only heap value is String object.
class ImmutableGC VCL_FINAL : public GC {
 public:
  ImmutableGC() : GC(1, 1.0) {}
  ~ImmutableGC() {}

  using GC::gc_size;

  String* NewString(const char* str) {
    String* ret = New<String>(str);
    ret->set_black();
    return ret;
  }
  String* NewString(const std::string& str) {
    String* ret = New<String>(str);
    ret->set_black();
    return ret;
  }

  ACL* NewACL(vm::IPPattern* pattern) {
    ACL* ret = New<ACL>(pattern);
    ret->set_black();
    return ret;
  }

  Module* NewModule(const std::string& name) {
    Module* mod = New<Module>(name);
    mod->set_black();
    return mod;
  }

 private:
  virtual void Mark() {}

  VCL_DISALLOW_COPY_AND_ASSIGN(ImmutableGC);
};

namespace detail {

// A envrionment object is sololy used by Context and Engine object to
// support global variable and other customized extension support.
template <typename T, typename GCType>
class Environment {
 public:
  ~Environment();
  // Extension
  bool RegisterExtensionFactory(const std::string&, ExtensionFactory*);
  bool RemoveExtensionFactory(const std::string&);
  void ClearExtensionFactory();

  ExtensionFactory* GetExtensionFactory(const std::string&) const;
  ExtensionFactory* GetExtensionFactory(const char*) const;
  ExtensionFactory* GetExtensionFactory(const String&) const;

  // Module
  Module* AddModule(const std::string&);
  bool AddModule(Module*);
  Module* RemoveModule(const std::string&);
  void ClearModule() { m_mod_map.Clear(); }

  Module* GetModule(const std::string&) const;
  Module* GetModule(const char*) const;
  Module* GetModule(const String&) const;

  // Global variables
  void AddOrUpdateGlobalVariable(const std::string& name, const Value& value);
  void AddOrUpdateGlobalVariable(const char* name, const Value& value);
  void AddOrUpdateGlobalVariable(const String& name, const Value& value);

  bool AddGlobalVariable(const std::string&, const Value&);
  bool AddGlobalVariable(const char*, const Value&);
  bool AddGlobalVariable(const String&, const Value&);

  bool GetGlobalVariable(const std::string& name, Value*) const;
  bool GetGlobalVariable(const char* name, Value*) const;
  bool GetGlobalVariable(const String&, Value*) const;

  bool RemoveGlobalVariable(const std::string& name);
  // Clear all global variables
  void ClearGlobalVariables();

  size_t GlobalVariableSize() const { return m_gvar_map.size(); }

 protected:
  Environment() : m_gvar_map(), m_ext_map(), m_mod_map() {}

  void Mark();

 private:
  GCType* gc() { return static_cast<T*>(this)->gc(); }

  typedef StringDict<ExtensionFactory*> ExtensionMap;
  typedef StringDict<Value> GVarMap;
  typedef StringDict<Module*> ModuleMap;

  // Global variable map
  GVarMap m_gvar_map;

  // ExtensionFactory map
  ExtensionMap m_ext_map;

  // Module map
  ModuleMap m_mod_map;
};

}  // namespace detail

// Represents an Compiled source code as with all its related resources. If an
// execution needs to existed, the CompiledCode must existed. Each context
// object
// will have a shared pointer points to the compiled code object internally. But
// if user want to have new context object then it must do so via CompiledCode
// object.
class CompiledCode VCL_FINAL
    : public boost::enable_shared_from_this<CompiledCode> {
 public:
  CompiledCode(Engine* engine);
  ~CompiledCode();

 public:
  uint32_t AddSourceCodeInfo(const boost::shared_ptr<SourceCodeInfo>&);
  boost::shared_ptr<SourceCodeInfo> IndexSourceCodeInfo(uint32_t) const;

 public:
  vm::Procedure* entry() const { return m_entry; }
  Engine* engine() const { return m_engine; }
  ImmutableGC* gc() const { return &m_gc; }

 public:
  // Debug
  void Dump(std::ostream&) const;

 private:
  // A list of SourceCode object that contributes to this CompiledCode
  // object
  std::vector<boost::shared_ptr<SourceCodeInfo> > m_source_code_list;
  boost::ptr_vector<vm::Procedure> m_sub_routine_list;
  vm::Procedure* m_entry;
  Engine* m_engine;
  mutable ImmutableGC m_gc;

  friend class CompiledCodeBuilder;
  VCL_DISALLOW_COPY_AND_ASSIGN(CompiledCode);
};

struct ContextOption {
  // Max calling stack size for virtual machine
  size_t max_calling_stack_size;
  // GC trigger time
  size_t gc_trigger;
  // GC ratio
  double gc_ratio;
  // Maximum Gap
  size_t gc_maximum_gap;

  ContextOption()
      : max_calling_stack_size(16),
        gc_trigger(1000),
        gc_ratio(0.5),
        gc_maximum_gap(0) {}
};

class Context VCL_FINAL : public detail::Environment<Context, ContextGC> {
  typedef detail::Environment<Context, ContextGC> Base;

 public:
  Context(const ContextOption&, const boost::shared_ptr<CompiledCode>&);

  ~Context();

  // Call construct to invoke the __ctor__ internal function
  MethodStatus Construct();

 public:
  Engine* engine() const { return m_compiled_code->engine(); }
  ContextGC* gc() const { return &m_gc; }

 public:  // Yield and Resume
  // This function will yield the current executing context *directly* from
  // virtual machine. This function can be called inside of a signal handler
  // which basically allow the user to execute virtual machine code in a
  // preemptive
  // fashion.
  //
  // However this is none trivial especially when the C++ code calls into the
  // script code. Since all script code can generate a Yield results back due
  // to the Yield call.
  bool Yield();

  // Check whether the Context object is yielded or not.
  bool is_yield() const;

  // If the context object is yielded, then call this function to resume the
  // previous call and returns the corresponding return value. Pay attention,
  // the Resume function can still return Yield states if the function have
  // multiple Yield operations built in.
  MethodStatus Resume(Value*);

 public:
  // Invoke a sub_routine defined in the script side with certain arguments
  // This API is not very efficient but may be good enough for us to integrate
  // it into our existed code base
  MethodStatus Invoke(SubRoutine*, const std::vector<Value>&, Value*);

  // The following APIs enumerate up to 8 arguments function call for
  // invoking a sub_routine. These APIs are faster because it avoids to
  // have a std::vector.
  MethodStatus Invoke(SubRoutine*, Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, const Value&,
                      Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, const Value&,
                      const Value&, Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, const Value&,
                      const Value&, const Value&, Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, const Value&,
                      const Value&, const Value&, const Value&, Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, const Value&,
                      const Value&, const Value&, const Value&, const Value&,
                      Value*);

  MethodStatus Invoke(SubRoutine*, const Value&, const Value&, const Value&,
                      const Value&, const Value&, const Value&, const Value&,
                      const Value&, Value*);

  // Get the argument size for the current function call
  size_t GetArgumentSize() const;

  // Get indexed argument value for the current function call
  Value GetArgument(size_t) const;

 public:
  // GC
  void Mark();

 public:  // Opaque pointers
  CompiledCode* compiled_code() const { return m_compiled_code.get(); }
  vm::Runtime* runtime() const { return m_runtime.get(); }

 private:
  // Runtime object related to this context
  boost::scoped_ptr<vm::Runtime> m_runtime;

  // Compiled code object
  boost::shared_ptr<CompiledCode> m_compiled_code;

  // Its own GC
  mutable ContextGC m_gc;

  friend class GC;
  friend class ContextGC;

  VCL_DISALLOW_COPY_AND_ASSIGN(Context);
};

// Tunable script loading options
struct ScriptOption {
  // Which folder to *resolve* include expression inside of the file path.
  std::string folder_hint;

  // Indicate how many includes should be used
  size_t max_include;

  // Whether we allow absolute path for include in this script
  bool allow_absolute_path;

  // Whether support for keyword
  bool allow_loop;

  ScriptOption()
      : folder_hint(),
        max_include(4),
        allow_absolute_path(false),
        allow_loop(true) {}
};

// A engine represents a central repository to store all the parsed/compiled
// code and other stuff. The engine doesn't mean runtime feature but just a
// place to hold shared resources.
class Engine VCL_FINAL : public detail::Environment<Engine, ImmutableGC> {
  typedef detail::Environment<Engine, ImmutableGC> Base;

 public:
  typedef ImmutableGC GCType;
  Engine();

  ImmutableGC* gc() const { return &m_gc; }

 public:
  // This is the core API to load a script. After loading the script, user
  // will get an shared object for CompiledCode. The CompiledCode object will
  // record all the needed compiled script resources. Via compiled code object,
  // user is able to get an context which is an isolated environment for script
  // interpretation.
  boost::shared_ptr<CompiledCode> LoadFile(const std::string&,
                                           const ScriptOption&,
                                           std::string* error);

  boost::shared_ptr<CompiledCode> LoadString(const std::string&,
                                             const std::string&,
                                             const ScriptOption&,
                                             std::string* error);

 private:
  mutable ImmutableGC m_gc;
  VCL_DISALLOW_COPY_AND_ASSIGN(Engine);
};

// =========================================================================
// Initialization APIs
// Call this API before using any VCL library classes or APIs
// =========================================================================
void InitVCL(const char* process_path, double vcl_version_tag = 4.0);

// Check VCL Version is correct or not. Use InitVCL to setup VCL version.
// This version is really useless right now since we just do a simple
// equal check and we don't support multiple version in current implementation.
bool CheckVCLVersion(double version);

// =========================================================================
// Inline APIs definition
// =========================================================================
inline const char* MethodStatus::status_name() const {
  switch (status()) {
    case METHOD_OK:
      return "ok";
    case METHOD_TERMINATE:
      return "terminate";
    case METHOD_FAIL:
      return "fail";
    case METHOD_YIELD:
      return "yield";
    case METHOD_UNIMPLEMENTED:
      return "unimplemented";
    default:
      VCL_UNREACHABLE();
      return NULL;
  }
}

inline void MethodStatus::set_fail(const char* format, ...) {
  if (status() != METHOD_FAIL) {
    m_storage = Fail();
  }
  std::string* buffer = &(boost::get<Fail>(m_storage)).reason;
  va_list vl;
  va_start(vl, format);
  vcl::util::FormatV(buffer, format, vl);
}

inline void MethodStatus::set_unimplemented(const char* format, ...) {
  if (status() != METHOD_UNIMPLEMENTED) {
    m_storage = Unimplemented();
  }
  std::string* buffer = &(boost::get<Unimplemented>(m_storage)).description;
  va_list vl;
  va_start(vl, format);
  vcl::util::FormatV(buffer, format, vl);
}

inline void MethodStatus::set_status(int s) {
  switch (s) {
    case METHOD_OK:
      m_storage = Ok();
      break;
    case METHOD_TERMINATE:
      m_storage = Terminate();
      break;
    case METHOD_FAIL:
      m_storage = Fail();
      break;
    case METHOD_YIELD:
      m_storage = Yield();
      break;
    case METHOD_UNIMPLEMENTED:
      m_storage = Unimplemented();
      break;
    default:
      VCL_UNREACHABLE();
      break;
  }
}

MethodStatus::MethodStatus(int status, const char* format, va_list vl)
    : m_storage() {
  std::string* buffer;
  if (status == METHOD_FAIL) {
    m_storage = Fail();
    buffer = &boost::get<Fail>(m_storage).reason;
  } else {
    m_storage = Unimplemented();
    buffer = &boost::get<Unimplemented>(m_storage).description;
  }
  vcl::util::FormatV(buffer, format, vl);
}

template <typename T>
inline Handle<T>::Handle(T* value, GC* gc)
    : m_value(value), m_gc(gc), m_iterator(gc->AddRoot(value)) {}

template <typename T>
inline Handle<T>::~Handle() {
  if (m_value) m_gc->RemoveRoot(m_iterator);
}

template <typename T>
inline void Handle<T>::Dispose() {
  if (m_value) m_gc->RemoveRoot(m_iterator);
  m_value = NULL;
}

template <typename T>
inline Handle<T>::Handle(const Handle<T>& h)
    : m_value(h.m_value), m_gc(h.m_gc), m_iterator(h.m_iterator) {
  if (!empty()) m_iterator->ref_count++;
}

template <typename T>
inline Handle<T>& Handle<T>::operator=(const Handle& h) {
  if (h != &this) {
    Dispose();
    m_value = h.m_value;
    m_gc = h.m_gc;
    m_iterator = h.m_iterator;
    if (!empty()) m_iterator->ref_count++;
  }
  return *this;
}

inline Handle<Value>::Handle(const Value& value, GC* gc)
    : m_value(value), m_gc(gc), m_iterator() {
  if (value.IsObject()) {
    m_iterator = gc->AddRoot(value.GetObject());
  } else {
    m_gc = NULL;
  }
}

inline Handle<Value>::~Handle() {
  if (m_gc) m_gc->RemoveRoot(m_iterator);
}

inline Handle<Value>& Handle<Value>::operator=(const Handle<Value>& h) {
  if (&h != this) {
    if (m_gc) m_gc->RemoveRoot(m_iterator);
    m_value = h.m_value;
    m_gc = h.m_gc;
    m_iterator = h.m_iterator;
    if (m_gc) m_iterator->ref_count++;
  }
  return *this;
}

inline Handle<Value>::Handle(const Handle<Value>& h)
    : m_value(h.m_value), m_gc(h.m_gc), m_iterator(h.m_iterator) {
  if (m_gc) {
    m_iterator->ref_count++;
  }
}

inline Value::Value(String* value) : m_value(value), m_type(TYPE_STRING) {}

inline Value::Value(List* value) : m_value(value), m_type(TYPE_LIST) {}

inline Value::Value(Dict* value) : m_value(value), m_type(TYPE_DICT) {}

inline Value::Value(ACL* value) : m_value(value), m_type(TYPE_ACL) {}

inline Value::Value(Function* value) : m_value(value), m_type(TYPE_FUNCTION) {}

inline Value::Value(Action* value) : m_value(value), m_type(TYPE_ACTION) {}

inline Value::Value(Extension* value)
    : m_value(value), m_type(TYPE_EXTENSION) {}

inline Value::Value(Module* value) : m_value(value), m_type(TYPE_MODULE) {}

inline Value::Value(SubRoutine* value)
    : m_value(value), m_type(TYPE_SUB_ROUTINE) {}

inline Value::Value(Iterator* value) : m_value(value), m_type(TYPE_ITERATOR) {}

template <typename E>
inline Value::Value(Handle<E>& h) : m_value(h.get()), m_type(h->type()) {}

template <typename E>
inline Value::Value(const Handle<E>& h) : m_value(h.get()), m_type(h->type()) {}

inline void Value::Mark() {
  if (IsObject()) {
    object()->Mark();
  }
}

inline String* Value::GetString() const {
  DCHECK(IsString());
  return static_cast<String*>(object());
}

inline ACL* Value::GetACL() const {
  DCHECK(IsACL());
  return static_cast<ACL*>(object());
}

inline List* Value::GetList() const {
  DCHECK(IsList());
  return static_cast<List*>(object());
}

inline Dict* Value::GetDict() const {
  DCHECK(IsDict());
  return static_cast<Dict*>(object());
}

inline Function* Value::GetFunction() const {
  DCHECK(IsFunction());
  return static_cast<Function*>(object());
}

inline Extension* Value::GetExtension() const {
  DCHECK(IsExtension());
  return static_cast<Extension*>(object());
}

inline Action* Value::GetAction() const {
  DCHECK(IsAction());
  return static_cast<Action*>(object());
}

inline Module* Value::GetModule() const {
  DCHECK(IsModule());
  return static_cast<Module*>(object());
}

inline SubRoutine* Value::GetSubRoutine() const {
  DCHECK(IsSubRoutine());
  return static_cast<SubRoutine*>(object());
}

inline Iterator* Value::GetIterator() const {
  DCHECK(IsIterator());
  return static_cast<Iterator*>(object());
}

inline void Value::SetString(String* value) {
  m_type = TYPE_STRING;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetACL(ACL* value) {
  m_type = TYPE_ACL;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetList(List* value) {
  m_type = TYPE_LIST;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetDict(Dict* value) {
  m_type = TYPE_DICT;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetFunction(Function* value) {
  m_type = TYPE_FUNCTION;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetExtension(Extension* value) {
  m_type = TYPE_EXTENSION;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetAction(Action* value) {
  m_type = TYPE_ACTION;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetModule(Module* value) {
  m_type = TYPE_MODULE;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetSubRoutine(SubRoutine* value) {
  m_type = TYPE_SUB_ROUTINE;
  m_value = static_cast<Object*>(value);
}

inline void Value::SetIterator(Iterator* value) {
  m_type = TYPE_ITERATOR;
  m_value = static_cast<Object*>(value);
}

inline void Value::CastSizeToValueNoLostPrecision(Context* context,
                                                  size_t value, Value* output) {
  if (value > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    output->SetReal(static_cast<double>(value));
  } else {
    output->SetInteger(static_cast<int32_t>(value));
  }
}

template <typename T, typename Hasher>
template <int OPTION>
typename StringDict<T, Hasher>::HashEntry* StringDict<T, Hasher>::FindEntry(
    const char* string, uint32_t full_hash) const {
  const size_t mask = m_entry.size() - 1;
  size_t index = full_hash & mask;
  HashEntry* entry = &(m_entry[index]);

  if (!(entry->used)) {
    if (OPTION == OPT_FIND) {
      return NULL;
    } else {
      return entry;
    }
  } else {
    do {
      if (entry->del) {
        if (OPTION == OPT_INSERT) return entry;
      } else if (entry->Equal(string, full_hash)) {
        return entry;
      }
      if (!entry->more) {
        break;
      }
      entry = &(m_entry[entry->next]);
    } while (true);

    if (OPTION == OPT_FIND)
      return NULL;  // no alive entry
    else {
      HashEntry* new_entry;
      uint32_t h = full_hash;
      uint32_t idx;

      // linear probing
      do {
        idx = (++h & mask);
        new_entry = &(m_entry[idx]);
      } while (new_entry->used);

      entry->more = 1;
      entry->next = idx;

      return new_entry;
    }
  }
}

template <typename T, typename Hasher>
bool StringDict<T, Hasher>::Insert(const String& string, const T& value) {
  uint32_t full_hash = HashKey(string.data(), string.size());
  if (m_entry.size() == m_used) Rehash();
  HashEntry* entry = FindEntry<OPT_INSERT>(string.data(), full_hash);

  if (entry->del) {
    entry->pair = ValueType(&string, value);
    entry->full_hash = full_hash;
    entry->del = 0;
  } else {
    if (entry->used) {
      DCHECK(*entry->pair.first == string);
      return false;
    } else {
      entry->pair = ValueType(&string, value);
      entry->full_hash = full_hash;
      entry->used = 1;
      ++m_used;
    }
  }

  ++m_size;
  return true;
}

template <typename T, typename Hasher>
template <typename ALLOC>
bool StringDict<T, Hasher>::Insert(ALLOC* gc, const char* string,
                                   const T& value) {
  Handle<String> key(gc->NewString(string), gc);
  return Insert(*key, value);
}

template <typename T, typename Hasher>
template <typename ALLOC>
bool StringDict<T, Hasher>::Insert(ALLOC* gc, const std::string& string,
                                   const T& value) {
  return Insert(gc, string.c_str(), value);
}

template <typename T, typename Hasher>
bool StringDict<T, Hasher>::Update(const String& string, const T& value) {
  uint32_t full_hash = HashKey(string.data(), string.size());

  if (m_entry.size() == m_used) Rehash();
  HashEntry* entry = FindEntry<OPT_INSERT>(string.data(), full_hash);
  if (entry->used && !entry->del) {
    entry->pair.second = value;
    entry->full_hash = full_hash;
    return true;
  }
  return false;
}

template <typename T, typename Hasher>
template <typename ALLOC>
bool StringDict<T, Hasher>::Update(ALLOC* gc, const char* string,
                                   const T& value) {
  uint32_t full_hash = HashKey(string, strlen(string));

  if (m_entry.size() == m_used) Rehash();
  HashEntry* entry = FindEntry<OPT_INSERT>(string, full_hash);
  if (entry->used && !entry->del) {
    entry->pair.second = value;
    entry->full_hash = full_hash;
    return true;
  }
  return false;
}

template <typename T, typename Hasher>
void StringDict<T, Hasher>::InsertOrUpdate(const String& key, const T& value) {
  uint32_t full_hash = HashKey(key.data(), key.size());

  if (m_entry.size() == m_used) Rehash();
  HashEntry* entry = FindEntry<OPT_INSERT>(key.data(), full_hash);
  entry->full_hash = full_hash;

  if (entry->used && !entry->del) {
    entry->pair.second = value;
  } else {
    if (!entry->used) {
      entry->pair = ValueType(&key, value);
      entry->used = 1;
      ++m_used;
    } else {
      entry->pair = ValueType(&key, value);
      entry->del = 0;
    }
  }

  ++m_size;
}

template <typename T, typename Hasher>
template <typename ALLOC>
void StringDict<T, Hasher>::InsertOrUpdate(ALLOC* gc, const char* string,
                                           const T& value) {
  uint32_t full_hash = HashKey(string, strlen(string));

  if (m_entry.size() == m_used) Rehash();
  HashEntry* entry = FindEntry<OPT_INSERT>(string, full_hash);
  entry->full_hash = full_hash;

  if (entry->used && !entry->del) {
    entry->pair.second = value;
  } else {
    String* key =
        gc->NewString(string);  // Do it here since this prevent GC mark
                                // hitting *this* dictionary with an empty
                                // slot
    if (!entry->used) {
      entry->pair = ValueType(key, value);
      entry->used = 1;
      ++m_used;
    } else {
      entry->pair = ValueType(key, value);
      entry->del = 0;
    }
  }

  ++m_size;
}

template <typename T, typename Hasher>
T* StringDict<T, Hasher>::Find(const String& key) const {
  HashEntry* entry =
      FindEntry<OPT_FIND>(key.data(), HashKey(key.data(), key.size()));
  return entry ? &(entry->pair.second) : NULL;
}

template <typename T, typename Hasher>
T* StringDict<T, Hasher>::Find(const char* string) const {
  HashEntry* entry =
      FindEntry<OPT_FIND>(string, HashKey(string, strlen(string)));
  return entry ? &(entry->pair.second) : NULL;
}

template <typename T, typename Hasher>
bool StringDict<T, Hasher>::Remove(const String& key, T* output) {
  HashEntry* entry =
      FindEntry<OPT_FIND>(key.data(), HashKey(key.data(), key.size()));
  if (entry) {
    if (output) *output = entry->pair.second;
    --m_size;
    entry->del = 1;
    return true;
  }

  return false;
}

template <typename T, typename Hasher>
bool StringDict<T, Hasher>::Remove(const char* string, T* output) {
  HashEntry* entry =
      FindEntry<OPT_FIND>(string, HashKey(string, strlen(string)));
  if (entry) {
    if (output) *output = entry->pair.second;
    --m_size;
    entry->del = 1;
    return true;
  }

  return false;
}

template <typename T, typename Hasher>
void StringDict<T, Hasher>::Rehash() {
  uint32_t size = 0;
  HashVector vec;

  vec.resize(m_entry.size() * 2);
  m_entry.swap(vec);

  for (size_t i = 0; i < vec.size(); ++i) {
    HashEntry& entry = vec[i];
    DCHECK(entry.used);
    if (entry.del) continue;

    HashEntry* new_entry =
        FindEntry<OPT_INSERT>(entry.pair.first->data(), entry.full_hash);
    DCHECK(!new_entry->used);
    new_entry->used = 1;
    new_entry->pair = entry.pair;
    new_entry->full_hash = entry.full_hash;
    ++size;
  }

  m_size = size;
  m_used = size;
}

template <typename T, typename Hasher>
void StringDict<T, Hasher>::Clear() {
  m_size = 0;
  m_used = 0;
  m_entry.clear();
  m_entry.resize(kDefaultCapSize);
}

namespace detail {

template <typename T>
struct DoGCMark {
  static void Do(T& value) { value.Mark(); }
};

template <typename T>
struct DoGCMark<T*> {
  static void Do(T* value) { value->Mark(); }
};

template <typename T>
struct DoGCMark<const T> {
  static void Do(const T& value) { const_cast<T&>(value).Mark(); }
};

template <typename T>
struct DoGCMark<const T*> {
  static void Do(const T* value) { const_cast<T*>(value)->Mark(); }
};

}  // namespace detail

template <typename T, typename Hasher>
void StringDict<T, Hasher>::DoGCMark(StringDict<T, Hasher>* dict) {
  for (Iterator itr = dict->Begin(); itr != dict->End(); ++itr) {
    detail::DoGCMark<const String*>::Do(itr->first);
    detail::DoGCMark<T>::Do(itr->second);
  }
}

template <typename T, typename Hasher>
void StringDict<T, Hasher>::DoDelete(StringDict<T, Hasher>* dict) {
  for (Iterator itr = dict->Begin(); itr != dict->End(); ++itr) {
    delete itr->second;
  }
}

// Leave this function out for future string internalization implementation
inline String* ContextGC::NewStringImpl(const char* string) {
  return New<String>(string);
}

namespace detail {

template <typename T, typename GCType>
bool Environment<T, GCType>::RegisterExtensionFactory(
    const std::string& name, ExtensionFactory* factory) {
  m_ext_map.InsertOrUpdate(gc(), name, factory);
  return true;
}

template <typename T, typename GCType>
bool Environment<T, GCType>::RemoveExtensionFactory(const std::string& name) {
  ExtensionFactory* output;
  if (m_ext_map.Remove(name, &output)) {
    delete output;
    return true;
  }
  return false;
}

template <typename T, typename GCType>
void Environment<T, GCType>::ClearExtensionFactory() {
  m_ext_map.Clear();
}

template <typename T, typename GCType>
ExtensionFactory* Environment<T, GCType>::GetExtensionFactory(
    const std::string& name) const {
  ExtensionFactory** factory = m_ext_map.Find(name);
  return factory ? *factory : NULL;
}

template <typename T, typename GCType>
ExtensionFactory* Environment<T, GCType>::GetExtensionFactory(
    const char* name) const {
  ExtensionFactory** factory = m_ext_map.Find(name);
  return factory ? *factory : NULL;
}

template <typename T, typename GCType>
ExtensionFactory* Environment<T, GCType>::GetExtensionFactory(
    const String& name) const {
  ExtensionFactory** factory = m_ext_map.Find(name);
  return factory ? *factory : NULL;
}

template <typename T, typename GCType>
void Environment<T, GCType>::AddOrUpdateGlobalVariable(const std::string& name,
                                                       const Value& value) {
  Handle<Value> v(value, gc());
  m_gvar_map.InsertOrUpdate(gc(), name, value);
}

template <typename T, typename GCType>
void Environment<T, GCType>::AddOrUpdateGlobalVariable(const char* name,
                                                       const Value& value) {
  Handle<Value> v(value, gc());
  m_gvar_map.InsertOrUpdate(gc(), name, value);
}

template <typename T, typename GCType>
void Environment<T, GCType>::AddOrUpdateGlobalVariable(const String& name,
                                                       const Value& value) {
  Handle<Value> v(value, gc());
  m_gvar_map.InsertOrUpdate(name, value);
}

template <typename T, typename GCType>
bool Environment<T, GCType>::AddGlobalVariable(const std::string& name,
                                               const Value& value) {
  Handle<Value> v(value, gc());
  return m_gvar_map.Insert(gc(), name, value);
}

template <typename T, typename GCType>
bool Environment<T, GCType>::AddGlobalVariable(const char* name,
                                               const Value& value) {
  Handle<Value> v(value, gc());
  return m_gvar_map.Insert(gc(), name, value);
}

template <typename T, typename GCType>
bool Environment<T, GCType>::AddGlobalVariable(const String& name,
                                               const Value& value) {
  Handle<Value> v(value, gc());
  return m_gvar_map.Insert(name, value);
}

template <typename T, typename GCType>
bool Environment<T, GCType>::GetGlobalVariable(const std::string& name,
                                               Value* output) const {
  Value* result = m_gvar_map.Find(name);
  if (result) {
    *output = *result;
    return true;
  }
  return false;
}

template <typename T, typename GCType>
bool Environment<T, GCType>::GetGlobalVariable(const char* name,
                                               Value* output) const {
  Value* result = m_gvar_map.Find(name);
  if (result) {
    *output = *result;
    return true;
  }
  return false;
}

template <typename T, typename GCType>
bool Environment<T, GCType>::GetGlobalVariable(const String& name,
                                               Value* output) const {
  Value* result = m_gvar_map.Find(name);
  if (result) {
    *output = *result;
    return true;
  }
  return false;
}

template <typename T, typename GCType>
bool Environment<T, GCType>::RemoveGlobalVariable(const std::string& name) {
  return m_gvar_map.Remove(name, NULL);
}

template <typename T, typename GCType>
void Environment<T, GCType>::ClearGlobalVariables() {
  m_gvar_map.Clear();
}

template <typename T, typename GCType>
Module* Environment<T, GCType>::AddModule(const std::string& name) {
  Handle<Module> module(gc()->NewModule(name), gc());
  m_mod_map.InsertOrUpdate(gc(), name, module.get());
  return module.get();
}

template <typename T, typename GCType>
bool Environment<T, GCType>::AddModule(Module* mod) {
  Handle<Module> module(mod, gc());
  return m_mod_map.Insert(gc(), module->name(), module.get());
}

template <typename T, typename GCType>
Module* Environment<T, GCType>::GetModule(const std::string& name) const {
  Module** module = m_mod_map.Find(name);
  return module ? *module : NULL;
}

template <typename T, typename GCType>
Module* Environment<T, GCType>::GetModule(const String& name) const {
  Module** module = m_mod_map.Find(name);
  return module ? *module : NULL;
}

template <typename T, typename GCType>
Module* Environment<T, GCType>::GetModule(const char* name) const {
  Module** module = m_mod_map.Find(name);
  return module ? *module : NULL;
}

template <typename T, typename GCType>
Module* Environment<T, GCType>::RemoveModule(const std::string& name) {
  return m_mod_map.Remove(name, NULL);
}

template <typename T, typename GCType>
void Environment<T, GCType>::Mark() {
  GVarMap::DoGCMark(&m_gvar_map);
  ModuleMap::DoGCMark(&m_mod_map);
}

template <typename T, typename GCType>
Environment<T, GCType>::~Environment() {
  ExtensionMap::DoDelete(&m_ext_map);
}

}  // namespace detail
}  // namespace vcl

#endif  // VCL_H_
