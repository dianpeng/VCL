#include "builtin.h"
#include <vcl/vcl.h>
#include <cmath>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/time.h>

namespace vcl {

namespace {

// =======================================================================
// Interally, we expose builtin functions as default global function
// and also we expose several builtin modules which resides in certain
// namespace.
// =======================================================================


// =======================================================================
// Builtin functions
// 1. type
// 2. to_string
// 3. to_integer
// 4. to_real
// 5. to_boolean
// 6. dump
// 7. println
// 8. min
// 9. max
// 10. loop
// =======================================================================
class FunctionType : public Function {
 public:
  FunctionType(): Function("type") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 1 ) {
      return MethodStatus::NewFail("function::type expects 1 argument!");
    }
    output->SetString( context->gc()->NewString(context->GetArgument(0).type_name()));
    return MethodStatus::kOk;
  }
};

class FunctionToString : public Function {
 public:
  FunctionToString():Function("to_string") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 1 ) {
      return MethodStatus::NewFail("function::to_string expects 1 argument!");
    }

    Value arg = context->GetArgument(0);
    String* pstring = NULL;

    if(Value::ConvertToString(context,arg,&pstring)) {
      output->SetString(pstring);
      return MethodStatus::kOk;
    }
    return MethodStatus::NewFail("function::to_string convert type %s to "
                                 "string failed!",arg.type_name());
  }
};

class FunctionToInteger : public Function {
 public:
  FunctionToInteger():Function("to_integer") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 1 ) {
      return MethodStatus::NewFail("function::to_integer expects 1 argument!");
    }

    Value arg = context->GetArgument(0);
    int32_t ival;
    if(Value::ConvertToInteger(context,arg,&ival)) {
      output->SetInteger(ival);
      return MethodStatus::kOk;
    }
    return MethodStatus::NewFail("function::to_integer convert type %s to "
                                 "integer failed!",arg.type_name());
  }
};

class FunctionToReal : public Function {
 public:
  FunctionToReal() : Function("to_real") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 1 ) {
      return MethodStatus::NewFail("function::to_real expects 1 argument!");
    }

    Value arg = context->GetArgument(0);
    double dval;
    if(Value::ConvertToReal(context,arg,&dval)) {
      output->SetReal(dval);
      return MethodStatus::kOk;
    }

    return MethodStatus::NewFail("function::to_real convert type %s to "
                                 "integer failed!",arg.type_name());
  }
};

class FunctionToBoolean : public Function {
 public:
  FunctionToBoolean() : Function("to_boolean") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 1 ) {
      return MethodStatus::NewFail("function::to_boolean expects 1 argument!");
    }
    bool bval;
    if(Value::ConvertToBoolean(context,context->GetArgument(0),&bval)) {
      output->SetBoolean(bval);
    } else {
      output->SetNull();
    }
    return MethodStatus::kOk;
  }
};

class FunctionDump: public Function {
 public:
  FunctionDump() : Function("dump") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    const size_t len = context->GetArgumentSize();
    for( size_t i = 0 ; i < len ; ++i ) {
      context->GetArgument(i).ToDisplay(context,&std::cerr);
      std::cerr<<" ";
    }
    std::cerr<<'\n';
    output->SetNull();
    return MethodStatus::kOk;
  }
};

class FunctionPrintln : public Function {
 public:
  FunctionPrintln() : Function("println") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    const size_t len = context->GetArgumentSize();
    for( size_t i = 0 ; i < len ; ++i ) {
      Value v = context->GetArgument(i);
      switch(v.type()) {
        case TYPE_INTEGER: std::cout<<v.GetInteger()<<' '; break;
        case TYPE_REAL: std::cout<<v.GetReal()<<' '; break;
        case TYPE_NULL: std::cout<<"<null> "; break;
        case TYPE_BOOLEAN: std::cout<< (v.GetBoolean() ? "true " : "false "); break;
        case TYPE_STRING: std::cout<< v.GetString()->data(); break;
        case TYPE_DURATION: std::cout<<vcl::util::Duration::ToString(v.GetDuration()); break;
        case TYPE_SIZE: std::cout<<vcl::util::Size::ToString(v.GetSize()); break;
        default: {
          std::string string;
          if(v.ToString(context,&string)) {
            std::cout<<string<<' ';
          } else {
            return MethodStatus::NewFail("function::println argument %zu with type %s "
                                         "cannot be printed, doesn't support ToString!",
                                         (i+1),
                                         v.type_name());
          }
          break;
        }
      }
    }
    std::cout<<'\n';
    output->SetNull();
    return MethodStatus::kOk;
  }
};

class FunctionMin : public Function {
 public:
  FunctionMin() : Function("min") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    const size_t len = context->GetArgumentSize();
    if( len == 0 ) {
      return MethodStatus::NewFail("function::min requires at least 1 argument!");
    } else if( len == 1 ) {
      *output = context->GetArgument(0);
      return MethodStatus::kOk;
    } else {
      Value current  = context->GetArgument(0);
      for( size_t i = 1 ; i < len ; ++i ) {
        Value v = context->GetArgument(i);
        bool result;
        if( v.Less(context,current,&result) ) {
          if(result) current = v;
        } else {
          return MethodStatus::NewFail("function::min %zu argument with type %s,"
                                       "cannot be compared with others!",
                                       (i+1),
                                       v.type_name());
        }
      }
      *output = current;
      return MethodStatus::kOk;
    }
  }
};

class FunctionMax : public Function {
 public:
  FunctionMax() : Function("max") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    const size_t len = context->GetArgumentSize();
    if( len == 0 ) {
      return MethodStatus::NewFail("function::max requires at least 1 argument!");
    } else if( len == 1 ) {
      *output = context->GetArgument(0);
      return MethodStatus::kOk;
    } else {
      Value current  = context->GetArgument(0);
      for( size_t i = 1 ; i < len ; ++i ) {
        Value v = context->GetArgument(i);
        bool result;
        if( v.Greater(context,current,&result) ) {
          if(result) current = v;
        } else {
          return MethodStatus::NewFail("function::max %zu argument with type %s,"
                                       "cannot be compared with others!",
                                       (i+1),
                                       v.type_name());
        }
      }
      *output = current;
      return MethodStatus::kOk;
    }
  }
};

class Loop : public Iterator {
 public:
  Loop( int start , int end , int steps ):
    m_start(start),
    m_end  (end),
    m_steps(steps)
  {}

  // Must call this function to check whether the loop will terminate
  bool Check() const {
    int diff = (m_end-m_start);
    int next_steps = (m_start + m_steps);
    if( (m_end-next_steps) < diff ) {
      return true;
    }
    return false;
  }

 public:
  virtual bool Has( Context* context ) const {
    VCL_UNUSED(context);
    return m_start < m_end;
  }
  virtual bool Next( Context* context ) {
    VCL_UNUSED(context);
    m_start += m_steps;
    return m_start < m_end;
  }
  virtual void GetKey(Context* context , Value* key ) const {
    VCL_UNUSED(context);
    key->SetInteger(m_start);
  }
  virtual void GetValue(Context* context , Value* val) const {
    VCL_UNUSED(context);
    val->SetInteger(m_start);
  }
 private:
  int m_start;
  int m_end;
  int m_steps;

  VCL_DISALLOW_COPY_AND_ASSIGN(Loop);
};

class ForeverLoop : public Iterator {
 public:
  ForeverLoop():m_index(0){}
 public:
  virtual bool Has( Context* context ) const {
    VCL_UNUSED(context);
    return true;
  }
  virtual bool Next( Context* context ) {
    VCL_UNUSED(context);
    ++m_index;
    return true;
  }
  virtual void GetKey(Context* context , Value* key) const {
    VCL_UNUSED(context);
    key->SetInteger(m_index);
  }
  virtual void GetValue(Context* context , Value* value) const {
    VCL_UNUSED(context);
    value->SetInteger(m_index);
  }
 private:
  int32_t m_index;
};

class FunctionLoop : public Function {
 public:
  FunctionLoop() : Function("loop") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() == 0) {
      output->SetIterator( context->gc()->New<ForeverLoop>() );
      return MethodStatus::kOk;
    } else {
      if(context->GetArgumentSize() == 2) {
        Value start = context->GetArgument(0);
        Value end   = context->GetArgument(1);
        if( !start.IsInteger() || !end.IsInteger() ) {
          return MethodStatus::NewFail("function::loop's can accept 0,2 or 3 "
                                       "arguments, and all the arguments must "
                                       "be integer");
        }
        Handle<Loop> itr( context->gc()->New<Loop>(
              start.GetInteger(),
              end.GetInteger(),
              1) , context->gc() );
        if(!itr->Check()) {
          return MethodStatus::NewFail("function::loop's argument forms a "
                                       "loop that never terminates, if that "
                                       "is your purpose, please use loop "
                                       "zero argument function version");
        }
        output->SetIterator( itr.get() );
        return MethodStatus::kOk;
      } else if(context->GetArgumentSize() == 3) {
        Value start = context->GetArgument(0);
        Value end   = context->GetArgument(1);
        Value step  = context->GetArgument(2);
        if( !start.IsInteger() || !end.IsInteger() || !step.IsInteger() ) {
          return MethodStatus::NewFail("function::loop's can accept 0,2 or 3 "
                                       "arguments, and all the arguments must "
                                       "be integer");
        }
        Handle<Loop> itr( context->gc()->New<Loop>(
              start.GetInteger(),
              end.GetInteger(),
              step.GetInteger()) , context->gc() );
        if(!itr->Check()) {
          return MethodStatus::NewFail("function::loop's argument forms a "
                                       "loop that never terminates, if that "
                                       "is your purpose, please use loop "
                                       "zero argument function version");
        }
        output->SetIterator( itr.get() );
        return MethodStatus::kOk;
      } else {
        return MethodStatus::NewFail("function::loop's can accept 0,2 or 3 "
                                     "arguments, and all the arguments must "
                                     "be integer");
      }
    }
  }
};

// =========================================================================
// List modules
// =========================================================================
namespace list {

class ListPush : public Function {
 public:
  ListPush() : Function("list.push") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 2 || !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.push requires 2 arguments, "
                                   "first argument must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    if( l->size() >= List::kMaximumListSize ) {
      return MethodStatus::NewFail("function::list.push cannot push more to list,"
                                   "the list is too long and you can have a list "
                                   "no longer than %zu",List::kMaximumListSize);
    }
    l->Push( context->GetArgument(1) );
    output->SetTrue();
    return MethodStatus::kOk;
  }
};

class ListPop  : public Function {
 public:
  ListPop() : Function("list.pop") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 || !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.pop requires 1 argument and "
                                   "it must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    if(!l->empty()) {
      l->Pop();
      output->SetNull();
      return MethodStatus::kOk;
    } else {
      return MethodStatus::NewFail("function::list.pop cannot pop on empty list!");
    }
  }
};

class ListIndex: public Function {
 public:
  ListIndex() : Function("list.index") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 2 ||
      (!context->GetArgument(0).IsList() ||
       !context->GetArgument(1).IsInteger())) {
      return MethodStatus::NewFail("function::list.index requires 2 arguments "
                                   "and first argument must be a list , second "
                                   "argument must be an integer");
    }
    List* l = context->GetArgument(0).GetList();
    size_t idx = static_cast<size_t>( context->GetArgument(1).GetInteger() );
    if( idx >= l->size() ) {
      return MethodStatus::NewFail("function::list.index index value out of boundary!");
    } else {
      *output = l->Index(idx);
      return MethodStatus::kOk;
    }
  }
};

class ListFront : public Function {
 public:
  ListFront() : Function("list.front") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 || !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.front requires 1 argument,"
                                   "first argument must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    if(l->empty()) {
      return MethodStatus::NewFail("function::list.front list is empty!");
    } else {
      *output = l->Index(0);
      return MethodStatus::kOk;
    }
  }
};

class ListBack : public Function {
 public:
  ListBack() : Function("list.back") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 || !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.back requires 1 argument,"
                                   "first argument must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    if(l->empty()) {
      return MethodStatus::NewFail("function::list.back list is empty!");
    } else {
      *output = l->Index(l->size()-1);
      return MethodStatus::kOk;
    }
  }
};

class ListSlice : public Function {
 public:
  ListSlice() : Function("list.slice") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 3 ||
      (!context->GetArgument(0).IsList() ||
       !context->GetArgument(1).IsInteger() ||
       !context->GetArgument(2).IsInteger())) {
      return MethodStatus::NewFail("function::list.slice requires 3 arguments,"
                                   "first argument must be a list,"
                                   "second and third argument must be a integer");
    }
    List* l = context->GetArgument(0).GetList();
    int32_t start = context->GetArgument(1).GetInteger();
    int32_t end = context->GetArgument(2).GetInteger();
    int32_t len = static_cast<int32_t>(l->size());

    // Clamp the value to be in valid range
    if(start <0) start = 0;
    if(start >= len) start = len;
    if(end <0) end = 0;
    if(end >= len) end = len;
    if( end < start ) end = start;

    Handle<List> ret( context->gc()->NewList( end - start ) , context->gc() );
    if(end-start) ret->Reserve(end-start);

    for( ; start < end ; ++start )
      ret->Push( l->Index(start) );

    output->SetList(ret.get());
    return MethodStatus::kOk;
  }
};

class ListRange: public Function {
 public:
  ListRange() : Function("list.range") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 3 ||
      ( !context->GetArgument(0).IsInteger() ||
        !context->GetArgument(1).IsInteger() ||
        !context->GetArgument(2).IsInteger())) {
      return MethodStatus::NewFail("function::list.range requires 3 arguments,"
                                   "first,second and third arguments must be "
                                   "integer!");
    }
    int32_t start = context->GetArgument(0).GetInteger();
    int32_t end   = context->GetArgument(1).GetInteger();
    int32_t step  = context->GetArgument(2).GetInteger();

    // Check whether the loop will stop or not
    if( std::abs( end - (start+step) ) >= std::abs( end-start ) ) {
      return MethodStatus::NewFail("function::list.range argument specified doesnt "
                                   "form a close range!");
    }

    int32_t count = std::abs( end - start ) / std::abs( step );
    if( static_cast<size_t>(count) >= List::kMaximumListSize ) {
      return MethodStatus::NewFail("function::list.range range is too large,you "
                                   "can only specify list no larger than %zu",
                                   List::kMaximumListSize);
    }

    Handle<List> l( context->gc()->NewList(static_cast<size_t>(count)) , context->gc() );
    for( ; start < end ; start += step ) {
      l->Push(Value(start));
    }
    output->SetList(l.get());
    return MethodStatus::kOk;
  }
};

class ListResize: public Function {
 public:
  ListResize() : Function("list.resize") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 2 ||
       (!context->GetArgument(0).IsList() ||
        !context->GetArgument(1).IsInteger())) {
      return MethodStatus::NewFail("function::list.resize requires 2 arguments "
                                   "and first argument must be a list , second "
                                   "argument must be an integer");
    }
    List* l = context->GetArgument(0).GetList();
    size_t sz = static_cast<size_t>( context->GetArgument(1).GetInteger() );
    if(sz >= List::kMaximumListSize) {
      return MethodStatus::NewFail("function::list.resize tries to resize too "
                                   "large array, currently you can only have "
                                   "%zu",List::kMaximumListSize);
    } else {
      l->Resize(sz);
      output->SetNull();
      return MethodStatus::kOk;
    }
  }
};

class ListClear: public Function {
 public:
  ListClear() : Function("list.clear") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
      (!context->GetArgument(0).IsList())) {
      return MethodStatus::NewFail("function::list.clear requires 1 argument,"
                                   "and first argument must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    l->Clear();
    output->SetNull();
    return MethodStatus::kOk;
  }
};

class ListSize : public Function {
 public:
  ListSize() : Function("list.size") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.size requires 1 argument,"
                                   "and first argument must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    output->SetInteger( static_cast<int32_t>( l->size() ) );
    return MethodStatus::kOk;
  }
};

class ListEmpty: public Function {
 public:
  ListEmpty() : Function( "list.empty" ) {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.empty requires 1 argument,"
                                   "and first argument must be a list");
    }
    List* l = context->GetArgument(0).GetList();
    output->SetBoolean( l->empty() );
    return MethodStatus::kOk;
  }
};

class ListJoin : public Function {
 public:
  ListJoin() : Function("list.join") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.join requires 1 argument,"
                                   "and it must be list");
    }
    List* l = context->GetArgument(0).GetList();
    if( l->size() == 0 ) {
      output->SetNull();
    } else {
      Value current = l->Index(0);
      for( size_t i = 1 ; i < l->size() ; ++i ) {
        Value v = l->Index(i);
        Value temp;
        if( current.Add( context , v , &temp ) ) {
          current = temp;
        } else {
          return MethodStatus::NewFail("function::list.join the %zuth element "
                                       "in list with type %s doesn't support "
                                       "Add operation with other elements!",
                                       (i+1),
                                       v.type_name());
        }
      }
      *output = current;
    }
    return MethodStatus::kOk;
  }
};

class ListMaxSize : public Function {
 public:
  ListMaxSize() : Function("list.max_size") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsList()) {
      return MethodStatus::NewFail("function::list.max_size requires 1 argument,"
                                   "and it must be list");
    }
    output->SetInteger( List::kMaximumListSize );
    return MethodStatus::kOk;
  }
};

template<typename T>
Module* CreateListModule( T* target ) {
  Handle<Module> handle(target->gc()->NewModule("list"),target->gc());

#define ADD(NAME,FUNC) \
  do { \
    Handle<String> key(target->gc()->NewString(NAME),target->gc()); \
    Handle<Function> val(target->gc()->template New<FUNC>(),target->gc()); \
    handle->AddProperty(*key,Value(val)); \
  } while(false)

  ADD("push",ListPush);
  ADD("pop",ListPop);
  ADD("index",ListIndex);
  ADD("front",ListFront);
  ADD("back",ListBack);
  ADD("slice",ListSlice);
  ADD("resize",ListResize);
  ADD("range",ListRange);
  ADD("clear",ListClear);
  ADD("size",ListSize);
  ADD("empty",ListEmpty);
  ADD("join",ListJoin);
  ADD("max_size",ListMaxSize);

#undef ADD // ADD

  return handle.get();
}
} // namespace list

// =========================================================================
// GC modules
// =========================================================================
namespace gc {

class FunctionGCSize : public Function {
 public:
  FunctionGCSize() : Function("gc.gc_size") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 0 ) {
      return MethodStatus::NewFail("function::gc.gc_size requires 0 argument");
    }
    Value::CastSizeToValueNoLostPrecision(context,
                                          context->gc()->gc_size(),
                                          output);
    return MethodStatus::kOk;
  }
};

class FunctionGCTrigger : public Function {
 public:
  FunctionGCTrigger() : Function("gc.gc_trigger") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 0 ) {
      return MethodStatus::NewFail("function::gc.gc_trigger requires 0 argument");
    }
    Value::CastSizeToValueNoLostPrecision(context,
                                          context->gc()->next_gc_trigger(),
                                          output);
    return MethodStatus::kOk;
  }
};

class FunctionGCTimes : public Function {
 public:
  FunctionGCTimes() : Function("gc.gc_times") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 0 ) {
      return MethodStatus::NewFail("function::gc.gc_times requires 0 argument");
    }
    output->SetInteger( static_cast<int32_t>( context->gc()->gc_times() ) );
    Value::CastSizeToValueNoLostPrecision(context,
                                          context->gc()->gc_times(),
                                          output);
    return MethodStatus::kOk;
  }
};

class FunctionGCRatio : public Function {
 public:
  FunctionGCRatio() : Function("gc.gc_ratio") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 0 ) {
      return MethodStatus::NewFail("function::gc.gc_ratio requires 0 argument");
    }
    output->SetReal(context->gc()->gc_ratio());
    return MethodStatus::kOk;
  }
};

class FunctionForceCollect: public Function {
 public:
  FunctionForceCollect() : Function("gc.force_collect") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 0 ) {
      return MethodStatus::NewFail("function::gc.force_collect requires 0 argument");
    }
    context->gc()->ForceCollect();
    output->SetNull();
    return MethodStatus::kOk;
  }
};

class FunctionTryCollect : public Function {
 public:
  FunctionTryCollect() : Function("gc.try_collect") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if( context->GetArgumentSize() != 0 ) {
      return MethodStatus::NewFail("function::gc.try_collect requires 0 argument");
    }
    output->SetBoolean( context->gc()->TryCollect() );
    return MethodStatus::kOk;
  }
};

template< typename T >
Module* CreateGCModule( T* engine ) {
  Handle<Module> module( engine->gc()->NewModule("gc") , engine->gc() );

#define ADD(NAME,FUNC) \
  do { \
    Handle<String> key( engine->gc()->NewString(NAME) , engine->gc() ); \
    Handle<Function> val( engine->gc()->template New<FUNC>()   , engine->gc() ); \
    module->AddProperty(*key,Value(val)); \
  } while(false)

  ADD("gc_size",FunctionGCSize);
  ADD("gc_times",FunctionGCTimes);
  ADD("gc_trigger",FunctionGCTrigger);
  ADD("gc_ratio",FunctionGCRatio);
  ADD("force_collect",FunctionForceCollect);
  ADD("try_collect",FunctionTryCollect);

#undef ADD // ADD

  return module.get();
}
} // namespace gc


// =========================================================================
// Dict modules
// =========================================================================
namespace dict {

class FunctionUpdate : public Function {
 public:
  FunctionUpdate() : Function("dict.update") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 3 ||
       !context->GetArgument(0).IsDict() ||
       !context->GetArgument(1).IsString()) {
      return MethodStatus::NewFail("function::dict.update expects 3 arguments, "
                                   "first argument must be a dictionary,"
                                   "second argument must be a string");
    }
    Dict* d = context->GetArgument(0).GetDict();
    String* k = context->GetArgument(1).GetString();
    d->InsertOrUpdate(*k,context->GetArgument(2));
    output->SetNull();
    return MethodStatus::kOk;
  }
};

class FunctionInsert : public Function {
 public:
  FunctionInsert() : Function("dict.insert") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 3 ||
       !context->GetArgument(0).IsDict() ||
       !context->GetArgument(1).IsString()) {
      return MethodStatus::NewFail("function::dict.insert expects 3 arguments,"
                                   "first argument must be a dictionary,"
                                   "second argument must be a string");
    }
    Dict* d  = context->GetArgument(0).GetDict();
    String*k = context->GetArgument(1).GetString();
    output->SetBoolean( d->Insert(*k,context->GetArgument(2)) );
    return MethodStatus::kOk;
  }
};

class FunctionFind : public Function {
 public:
  FunctionFind() : Function("dict.find") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
   if(context->GetArgumentSize() != 2 ||
      !context->GetArgument(0).IsDict() ||
      !context->GetArgument(1).IsString()) {
     return MethodStatus::NewFail("function::dict.find expects 2 arguments,"
                                  "first argument must be a dictionary,"
                                  "second argument must be a string");
   }
   Dict* d = context->GetArgument(0).GetDict();
   String* key = context->GetArgument(1).GetString();
   if(!d->Find(*key,output)) output->SetNull();
   return MethodStatus::kOk;
  }
};

class FunctionExist : public Function {
 public:
  FunctionExist(): Function("dict.exist") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 2 ||
       !context->GetArgument(0).IsDict() ||
       !context->GetArgument(1).IsString()) {
      return MethodStatus::NewFail("function::dict.exist expects 2 arguments,"
                                   "first argument must be a dictionary,"
                                   "second argument must be a string");
    }
    Dict* d = context->GetArgument(0).GetDict();
    String* k = context->GetArgument(1).GetString();
    Value dull;
    output->SetBoolean( d->Find(*k,&dull) );
    VCL_UNUSED(dull);
    return MethodStatus::kOk;
  }
};

class FunctionRemove : public Function {
 public:
  FunctionRemove() : Function("dict.remove") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 2 ||
       !context->GetArgument(0).IsDict() ||
       !context->GetArgument(1).IsString()) {
      return MethodStatus::NewFail("function::dict.remove expects 2 arguments,"
                                   "first argument must be a dictionary,"
                                   "second argument must be a string");
    }
    Dict* d = context->GetArgument(0).GetDict();
    String*k= context->GetArgument(1).GetString();
    output->SetBoolean(d->Remove(*k,NULL));
    return MethodStatus::kOk;
  }
};

class FunctionClear : public Function {
 public:
  FunctionClear() : Function("dict.clear") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1||
       !context->GetArgument(0).IsDict()) {
      return MethodStatus::NewFail("function::dict.clear expects 1 argument,"
                                   "and it must be a dictionary");
    }
    context->GetArgument(0).GetDict()->Clear();
    output->SetNull();
    return MethodStatus::kOk;
  }
};

class FunctionSize : public Function {
 public:
  FunctionSize() : Function("dict.size") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsDict()) {
      return MethodStatus::NewFail("function::dict.size expects 1 argument,"
                                   "and it must be a dictionary");
    }
    output->SetInteger( static_cast<int32_t>(
          context->GetArgument(0).GetDict()->size()));
    return MethodStatus::kOk;
  }
};

class FunctionEmpty: public Function {
 public:
  FunctionEmpty() : Function("dict.empty") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsDict()) {
      return MethodStatus::NewFail("function::dict.empty expects 1 argument,"
                                   "and it must be a dictionary");
    }
    output->SetBoolean( context->GetArgument(0).GetDict()->empty() );
    return MethodStatus::kOk;
  }
};

class FunctionMaxSize : public Function {
 public:
  FunctionMaxSize() : Function("dict.max_size") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsDict()) {
      return MethodStatus::NewFail("function::dict.max_size expects 1 argument,"
                                   "and it must be a dictionary");
    }
    output->SetInteger( Dict::kMaximumDictSize );
    return MethodStatus::kOk;
  }
};

template< typename T >
Module* CreateDictModule( T* engine ) {
  Handle<Module> module( engine->gc()->NewModule("dict") , engine->gc() );

#define ADD(NAME,FUNC) \
  do { \
    Handle<String> key( engine->gc()->NewString(NAME) , engine->gc() ); \
    Handle<Function> val( engine->gc()->template New<FUNC>() , engine->gc()); \
    module->AddProperty(*key,Value(val)); \
  } while(false)

  ADD("update",FunctionUpdate);
  ADD("insert",FunctionInsert);
  ADD("find",FunctionFind);
  ADD("exist",FunctionExist);
  ADD("remove",FunctionRemove);
  ADD("clear",FunctionClear);
  ADD("size",FunctionSize);
  ADD("empty",FunctionEmpty);
  ADD("max_size",FunctionMaxSize);

#undef ADD // ADD

  return module.get();
}
} // namespace dict

// =========================================================================
// Dict modules
// =========================================================================
namespace string {

class FunctionSize : public Function {
 public:
  FunctionSize() : Function("string.size") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.size expects 1 argument,"
                                   "and it must be string");
    }
    output->SetInteger( static_cast<int32_t>(
          context->GetArgument(0).GetString()->size()) );
    return MethodStatus::kOk;
  }
};

class FunctionEmpty : public Function {
 public:
  FunctionEmpty() : Function("string.empty") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.empty expects 1 argument,"
                                   "and it must be string");
    }
    output->SetBoolean( context->GetArgument(0).GetString()->empty() );
    return MethodStatus::kOk;
  }
};

class FunctionLeftTrim : public Function {
 public:
  FunctionLeftTrim() : Function("string.left_trim") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.left_trim expects 1 argument,"
                                   "and it must be string");
    }
    std::string target = context->GetArgument(0).GetString()->ToStdString();
    boost::algorithm::trim_left(target);
    output->SetString( context->gc()->NewString(target) );
    return MethodStatus::kOk;
  }
};

class FunctionRightTrim : public Function {
 public:
  FunctionRightTrim(): Function("string.right_trim") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.right_trim expects 1 argument,"
                                   "and it must be string");
    }
    std::string target = context->GetArgument(0).GetString()->ToStdString();
    boost::algorithm::trim_right(target);
    output->SetString( context->gc()->NewString(target) );
    return MethodStatus::kOk;
  }
};

class FunctionTrim: public Function {
 public:
  FunctionTrim(): Function("string.trim") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.trim expects 1 argument,"
                                   "and it must be string");
    }
    std::string target = context->GetArgument(0).GetString()->ToStdString();
    boost::algorithm::trim(target);
    output->SetString( context->gc()->NewString(target) );
    return MethodStatus::kOk;
  }
};

class FunctionDup : public Function {
 public:
  FunctionDup() : Function("string.dup") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.dup expects 1 argument,"
                                   "and it must be string");
    }
    output->SetString( context->gc()->NewString(
          context->GetArgument(0).GetString()->data()) );
    return MethodStatus::kOk;
  }
};

class FunctionUpper : public Function {
 public:
  FunctionUpper() : Function("string.upper") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.upper expects 1 argument,"
                                   "and it must be string");
    }
    output->SetString(context->gc()->NewString(
          boost::algorithm::to_upper_copy<std::string>(
            context->GetArgument(0).GetString()->data())));
    return MethodStatus::kOk;
  }
};

class FunctionLower : public Function {
 public:
  FunctionLower() : Function("string.lower") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 1 ||
       !context->GetArgument(0).IsString()) {
      return MethodStatus::NewFail("function::string.lower expects 1 argument,"
                                   "and it must be string");
    }
    output->SetString(context->gc()->NewString(
          boost::algorithm::to_lower_copy<std::string>(
            context->GetArgument(0).GetString()->data())));
    return MethodStatus::kOk;
  }
};

class FunctionSlice : public Function {
 public:
  FunctionSlice() : Function("string.slice") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 3 ||
       !context->GetArgument(0).IsString() ||
       !context->GetArgument(1).IsInteger() ||
       !context->GetArgument(2).IsInteger()) {
      return MethodStatus::NewFail("function::string.slice expects 3 argument,"
                                   "first argument must be string,"
                                   "second and third argument must be integer");
    }
    String* str = context->GetArgument(0).GetString();
    int32_t len  = static_cast<int32_t>( str->size() );
    int32_t start= context->GetArgument(1).GetInteger();
    int32_t end  = context->GetArgument(2).GetInteger();

    // Clamp the value to be in valid range
    if(start <0) start = 0;
    if(start >= len) start = len;
    if(end <0) end = 0;
    if(end >= len) end = len;
    if( end < start ) end = start;
    output->SetString( context->gc()->NewString(
          str->ToStdString().substr(start,(end-start))));
    return MethodStatus::kOk;
  }
};

class FunctionIndex : public Function {
 public:
  FunctionIndex() : Function("string.index") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 2 ||
       !context->GetArgument(0).IsString() ||
       !context->GetArgument(1).IsInteger()) {
      return MethodStatus::NewFail("function::string.index expects 2 argument,"
                                   "first argument must be string,"
                                   "second argument must be integer");
    }
    String* str = context->GetArgument(0).GetString();
    int32_t index = context->GetArgument(1).GetInteger();
    int32_t len = static_cast<int32_t>( str->size() );
    if( index >= len || index < 0 ) {
      return MethodStatus::NewFail("function::string.index out of bound!");
    }
    char buf[2];
    buf[0] = str->data()[index];
    buf[1] = 0;
    output->SetString(context->gc()->NewString(buf));
    return MethodStatus::kOk;
  }
};

template< typename T >
Module* CreateStringModule( T* engine ) {
  Handle<Module> module( engine->gc()->NewModule("string") , engine->gc() );
#define ADD(NAME,FUNC) \
  do {  \
    Handle<String> key( engine->gc()->NewString(NAME) , engine->gc() ); \
    Handle<Function>val(engine->gc()->template New<FUNC>(),engine->gc()); \
    module->AddProperty(*key,Value(val)); \
  } while(false)

  ADD("size",FunctionSize);
  ADD("empty",FunctionEmpty);
  ADD("left_trim",FunctionLeftTrim);
  ADD("right_trim",FunctionRightTrim);
  ADD("trim",FunctionTrim);
  ADD("upper",FunctionUpper);
  ADD("lower",FunctionLower);
  ADD("dup",FunctionDup);
  ADD("slice",FunctionSlice);
  ADD("index",FunctionIndex);

#undef ADD // ADD
  return module.get();
}

} // namespace string


namespace time {

class FunctionNowInMicroSeconds : public Function {
 public:
  FunctionNowInMicroSeconds() : Function("time.now_in_micro_seconds") {}
  virtual MethodStatus Invoke( Context* context , Value* output ) {
    if(context->GetArgumentSize() != 0) {
      return MethodStatus::NewFail("function::time.now_in_micro_seconds expects "
                                   "no arguments");
    }
    {
      struct timespec tv;
      clock_gettime(CLOCK_MONOTONIC,&tv);
      output->SetInteger(static_cast<int32_t>(tv.tv_sec*1000000 + tv.tv_nsec / 1000));
    }
    return MethodStatus::kOk;
  }
};

template< typename T >
Module* CreateTimeModule( T* engine ) {
  Handle<Module> module( engine->gc()->NewModule("time") , engine->gc() );

#define ADD(NAME,FUNC) \
  do {  \
    Handle<String> key( engine->gc()->NewString(NAME) , engine->gc() ); \
    Handle<Function>val(engine->gc()->template New<FUNC>(),engine->gc()); \
    module->AddProperty(*key,Value(val)); \
  } while(false)

  ADD("now_in_micro_seconds",FunctionNowInMicroSeconds);

#undef ADD // ADD

  return module.get();
}

} // namespace time


template< typename T >
void Setup( T* engine ) {

  // 1. Global functions
#define ADD(NAME,FUNC) \
  do { \
    engine->AddOrUpdateGlobalVariable( NAME , Value(engine->gc()->template New<FUNC>()) ); \
  } while(false)

  ADD("type",FunctionType);
  ADD("to_string",FunctionToString);
  ADD("to_integer",FunctionToInteger);
  ADD("to_real",FunctionToReal);
  ADD("to_boolean",FunctionToBoolean);
  ADD("dump",FunctionDump);
  ADD("println",FunctionPrintln);
  ADD("min",FunctionMin);
  ADD("max",FunctionMax);
  ADD("loop",FunctionLoop);

#undef ADD // ADD

  // 2. Global modules
  engine->AddOrUpdateGlobalVariable( "list" , Value(list::CreateListModule(engine)));
  engine->AddOrUpdateGlobalVariable( "gc"   , Value(gc::CreateGCModule(engine)));
  engine->AddOrUpdateGlobalVariable( "dict" , Value(dict::CreateDictModule(engine)));
  engine->AddOrUpdateGlobalVariable( "string",Value(string::CreateStringModule(engine)));
  engine->AddOrUpdateGlobalVariable( "time" , Value(time::CreateTimeModule(engine)));
}

} // namespace

void AddBuiltin( Engine* engine ) { Setup<Engine>(engine); }

} // namespace vcl
