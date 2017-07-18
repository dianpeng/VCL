#ifndef RUNTIME_H_
#define RUNTIME_H_
#include <limits>

#include <vcl/util.h>
#include "procedure.h"
#include "vcl-pri.h"

#include <boost/variant.hpp>
#include <vector>

namespace vcl {
namespace vm {
class Runtime;

namespace detail {

class VMGuard {
 public:
  inline VMGuard(Runtime* runtime);
  inline ~VMGuard();

 private:
  Runtime* m_runtime;
};

}  // namespace detail

// A runtime in VCL represents the virtual machine instance plus all runtime
// variable related to the execution. The main virtual machine entry is in the
// Main function body and it is called by Run function.
// We have a typical stack based virtual machine, the memory layout is very
// simple since no loop involved , no need to optimize for performance. The
// frame for function call are stored seperately in an vector ; and the value
// stack is just a stack of Value objects. The Frame structure records
// everything
// and is able to resume or suspend a certain function execution on demand,
// which
// is part of our design.
class Runtime {
  // Default value stack initialized size
  static const size_t kDefaultValueStackSize = 512;

  // A Frame represents a calling frame inside of the call stack , it records
  // all runtime information about a function call, regardless of it is a c
  // function or script function
  struct Frame {
    // Base pointer , pointed at the start of the stack
    size_t base;

    // Size of argument has been pushed into the stack
    size_t arg_size;

    // Instruction pointer points to *next* bytecode
    size_t pc;

    // Which function is called
    Value caller;

    // Source code information index
    uint32_t source_index;

    // Instruction count
    int64_t instr_count;

    bool IsCppFunction() const { return caller.IsFunction(); }

    bool IsScriptFunction() const { return caller.IsSubRoutine(); }

    Function* function() const { return caller.GetFunction(); }

    SubRoutine* sub_routine() const { return caller.GetSubRoutine(); }

    Frame(size_t b, size_t sz, const Value& c)
        : base(b),
          arg_size(sz),
          pc(0),
          caller(c),
          source_index(0),
          instr_count(0) {}

    // For GC purpose
    void Mark() { caller.Mark(); }
  };

 public:
  Runtime(Context* context, int max_calling_stack_size)
      : m_context(context),
        m_max_calling_stack_size(max_calling_stack_size),
        m_frame(),
        m_stack(),
        m_v0(),
        m_v1(),
        m_yield(false),
        m_vm_running(false) {
    m_stack.reserve(kDefaultValueStackSize);
  }

 public:  // This a stateful APIs which is sololy used by Context object.
          // In most case you should not use the following APIs since it
          // is not safe at all
  MethodStatus BeginRun(SubRoutine*);
  void AddArgument(const Value&);
  MethodStatus FinishRun(SubRoutine*, Value*);

  // For getting the information about the function call and its argument
  size_t GetArgumentSize() const {
    const Frame* frame = CurrentFrame();
    DCHECK(frame);
    return frame->arg_size;
  }
  Value GetArgument(size_t index) const {
    size_t arg_size = GetArgumentSize();
    DCHECK(index < arg_size);
    DCHECK(!m_frame.empty());
    return Back(CurrentFrame()->base, index);
  }

 public:
  bool Yield() {
    if (m_vm_running) {
      m_yield = true;
      return true;
    }
    return false;
  }
  bool is_yield() const { return m_yield; }
  MethodStatus Resume(Value*);

 public:  // GC stuff
  void Mark();

  Context* context() const { return m_context; }
  ContextGC* gc() const { return m_context->gc(); }
  Engine* engine() const { return m_context->engine(); }

 private:
  // Unwind the current calling stack
  void UnwindStack(std::ostringstream*) const;

  MethodStatus ReportError(const std::string&) const;

  const Frame* CurrentFrame() const { return &m_frame.back(); }
  Frame* CurrentFrame() { return &m_frame.back(); }

  // Helper functions
  void Push(const Value& value) { m_stack.push_back(value); }
  void Pop(size_t count) {
    DCHECK(count <= m_stack.size());
    m_stack.resize(m_stack.size() - count);
  }
  void Replace(const Value& value) { m_stack.back() = value; }
  Value& Back(size_t base, size_t index) {
    DCHECK(index + base < m_stack.size());
    return m_stack[index + base];
  }
  Value& Top(size_t index) {
    DCHECK(index < m_stack.size());
    const size_t idx = (m_stack.size() - index - 1);
    return m_stack[idx];
  }
  const Value& Back(size_t base, size_t index) const {
    DCHECK(index + base < m_stack.size());
    return m_stack[index + base];
  }
  const Value& Top(size_t index) const {
    DCHECK(index < m_stack.size());
    const size_t idx = (m_stack.size() - index - 1);
    return m_stack[idx];
  }

  // Helper class for resolving those dependencies
  ExtensionFactory* GetExtensionFactory(const String& name) const {
    ExtensionFactory* factory = context()->GetExtensionFactory(name);
    if (!factory) {
      factory = engine()->GetExtensionFactory(name);
    }
    return factory;
  }

  Module* GetModule(const String& name) const {
    Module* module = context()->GetModule(name);
    if (!module) {
      module = engine()->GetModule(name);
    }
    return module;
  }

  bool GetGlobalVariable(const String& name, Value* output) const {
    if (!context()->GetGlobalVariable(name, output))
      return engine()->GetGlobalVariable(name, output);
    return true;
  }

  // Calling stack manipulation
  enum { FUNC_SCRIPT, FUNC_CPP, FUNC_FAILED };

  int EnterFunction(const Value&, size_t, MethodStatus* status);

  // Return true means continue interpreting, otherwise we are exit
  // from the outermost function, so should really exit from the current
  // frame
  bool ExitFunction(const Value&);

  // Reset the internal status of VM , this can only be called internally
  void Reset() {
    m_frame.clear();
    m_stack.clear();
    m_v0.SetNull();
  }

 private:
  // virtual machine entry
  MethodStatus Main(Value*,
                    int64_t count = std::numeric_limits<int64_t>::max());

  // Context that is belonged to this Runtime object
  Context* m_context;

  // Maximum calling stack size. If larger than this , throw overflow
  int m_max_calling_stack_size;

  // Function call's frame
  std::vector<Frame> m_frame;

  // Value stack
  std::vector<Value> m_stack;

  // Single scratch register simulation. Also it gets GCed, so anything
  // inside of m_v0 is safe to be used by C++ side code
  Value m_v0;

  // Another scratch register sololy for faster Scanning phase if GC kicks in
  Value m_v1;

  // Flag to tell whether we are yielded or not
  bool m_yield;

  // Flag to tell whether the VM is still running or not
  bool m_vm_running;

  friend class detail::VMGuard;
};

namespace detail {

inline VMGuard::VMGuard(Runtime* runtime) : m_runtime(runtime) {
  runtime->m_vm_running = true;
}

inline VMGuard::~VMGuard() { m_runtime->m_vm_running = false; }

}  // namespace detail
}  // namespace vm
}  // namespace vcl

#endif  // RUNTIME_H_
