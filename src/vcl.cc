#include <vcl/vcl.h>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/range.hpp>
#include <boost/type_traits.hpp>

#include "vm/compilation-unit.h"
#include "vm/compiler.h"
#include "vm/ip-address.h"
#include "vm/procedure.h"
#include "vm/runtime.h"
#include "vm/vcl-pri.h"

#include "lib/builtin.h"

namespace vcl {

const MethodStatus MethodStatus::kOk(MethodStatus::METHOD_OK);
const MethodStatus MethodStatus::kTerminate(MethodStatus::METHOD_TERMINATE);
const MethodStatus MethodStatus::kFail(MethodStatus::METHOD_FAIL);
const MethodStatus MethodStatus::kYield(MethodStatus::METHOD_YIELD);
const MethodStatus MethodStatus::kUnimplemented(
    MethodStatus::METHOD_UNIMPLEMENTED);

MethodStatus Value::GetProperty(Context* context, const String& key,
                                Value* output) const {
  if (IsObject()) {
    return object()->GetProperty(context, key, output);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support \".\" operator!", type_name());
  }
}

MethodStatus Value::SetProperty(Context* context, const String& key,
                                const Value& value) {
  if (IsObject()) {
    return object()->SetProperty(context, key, value);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support \".\" operator!", type_name());
  }
}

MethodStatus Value::GetAttribute(Context* context, const String& key,
                                 Value* output) const {
  if (IsObject()) {
    return object()->GetAttribute(context, key, output);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support \":\" operator!", type_name());
  }
}

MethodStatus Value::SetAttribute(Context* context, const String& key,
                                 const Value& value) {
  if (IsObject()) {
    return object()->SetAttribute(context, key, value);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support \":\" operator!", type_name());
  }
}

MethodStatus Value::GetIndex(Context* context, const Value& index,
                             Value* output) const {
  if (IsObject()) {
    return object()->GetIndex(context, index, output);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support \"[]\" operator!", type_name());
  }
}

MethodStatus Value::SetIndex(Context* context, const Value& index,
                             const Value& value) {
  if (IsObject()) {
    return object()->SetIndex(context, index, value);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support \"[]\" operator!", type_name());
  }
}

MethodStatus Value::Invoke(Context* context, Value* output) {
  if (IsObject()) {
    return object()->Invoke(context, output);
  } else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s doesn't support invoke as a function!", type_name());
  }
}

namespace {

struct AddOp {
  template <typename T>
  bool CheckRHS(const T& rhs) const {
    VCL_UNUSED(rhs);
    return true;
  }

  template <typename T>
  T Do(const T& left, const T& right) const {
    return left + right;
  }
};

struct SubOp {
  template <typename T>
  bool CheckRHS(const T& rhs) const {
    VCL_UNUSED(rhs);
    return true;
  }

  template <typename T>
  T Do(const T& left, const T& right) const {
    return left - right;
  }
};

struct MulOp {
  template <typename T>
  bool CheckRHS(const T& rhs) const {
    VCL_UNUSED(rhs);
    return true;
  }

  template <typename T>
  T Do(const T& left, const T& right) const {
    return left * right;
  }
};

struct DivOp {
  template <typename T>
  bool CheckRHS(const T& rhs) const {
    return rhs != 0;
  }

  template <typename T>
  T Do(const T& left, const T& right) const {
    return left / right;
  }
};

template <typename T>
T DoMod(T l, T r) {
  return l % r;
}

double DoMod(const double& l, const double& r) {
  VCL_UNUSED(l);
  VCL_UNUSED(r);
  return 0;
}

struct ModOp {
  template <typename T>
  bool CheckRHS(const T& rhs) const {
    return rhs != 0;
  }

  template <typename T>
  T Do(const T& left, const T& right) const {
    return DoMod(left, right);
  }
};

template <typename T>
bool OperatorImpl(Context* context, const Value& left, const Value& right,
                  Value* output, MethodStatus* status) {
  T handler;
  switch (left.type()) {
    case TYPE_INTEGER: {
      int32_t lhs = left.GetInteger();

      // Check if we need to do type promotion
      if (right.IsReal()) {
        if (!handler.CheckRHS(right.GetReal())) {
          status->set_fail("divide 0");
          return true;
        }
        output->SetReal(handler.Do(static_cast<double>(lhs), right.GetReal()));
      } else {
        int32_t value;
        if (!(*status = right.ToInteger(context, &value))) return false;
        if (!handler.CheckRHS(value)) {
          status->set_fail("divide 0");
          return true;
        }
        output->SetInteger(handler.Do(left.GetInteger(), value));
      }

      *status = MethodStatus::kOk;
      return true;
    }
    case TYPE_REAL: {
      double value;
      if (!(*status = right.ToReal(context, &value))) return false;
      if (!handler.CheckRHS(value)) {
        status->set_fail("divide 0");
        return true;
      }
      output->SetReal(handler.Do(left.GetReal(), value));
      *status = MethodStatus::kOk;
      return true;
    }
    case TYPE_NULL:
    case TYPE_SIZE:
    case TYPE_DURATION:
      return MethodStatus::NewUnimplemented(
          "Arithmatic operator cannot work between type "
          "%s and %s",
          left.type_name(), right.type_name());
    case TYPE_BOOLEAN: {
      if (right.IsReal()) {
        if (!handler.CheckRHS(right.GetReal())) {
          status->set_fail("divide 0");
          return true;
        }
        output->SetReal(
            handler.Do(static_cast<double>(left.GetBoolean() ? 1.0 : 0.0),
                       right.GetReal()));
      } else {
        int32_t value;
        if (!(*status = right.ToInteger(context, &value))) return false;
        if (!handler.CheckRHS(value)) {
          status->set_fail("divide 0");
          return true;
        }
        output->SetInteger(
            handler.Do(static_cast<int32_t>(left.GetBoolean() ? 1 : 0), value));
      }
      *status = MethodStatus::kOk;
      return true;
    }
    default:
      return false;
  }
}

}  // namespace

MethodStatus Value::Add(Context* context, const Value& value,
                        Value* output) const {
  MethodStatus status;
  if (OperatorImpl<AddOp>(context, *this, value, output, &status))
    return status;
  else
    return object()->Add(context, value, output);
}

MethodStatus Value::Sub(Context* context, const Value& value,
                        Value* output) const {
  MethodStatus status;
  if (OperatorImpl<SubOp>(context, *this, value, output, &status))
    return status;
  else
    return object()->Sub(context, value, output);
}

MethodStatus Value::Mul(Context* context, const Value& value,
                        Value* output) const {
  MethodStatus status;
  if (OperatorImpl<MulOp>(context, *this, value, output, &status))
    return status;
  else
    return object()->Mul(context, value, output);
}

MethodStatus Value::Div(Context* context, const Value& value,
                        Value* output) const {
  MethodStatus status;
  if (OperatorImpl<DivOp>(context, *this, value, output, &status))
    return status;
  else
    return object()->Div(context, value, output);
}

MethodStatus Value::Mod(Context* context, const Value& value,
                        Value* output) const {
  MethodStatus status;
  if (IsReal() || value.IsReal()) {
    return MethodStatus::NewFail("mod operator doesn't work with real number");
  }
  if (OperatorImpl<ModOp>(context, *this, value, output, &status))
    return status;
  else
    return object()->Mod(context, value, output);
}

MethodStatus Value::SelfAdd(Context* context, const Value& value) {
  MethodStatus status;
  if (OperatorImpl<AddOp>(context, *this, value, this, &status))
    return status;
  else
    return object()->SelfAdd(context, value);
}

MethodStatus Value::SelfSub(Context* context, const Value& value) {
  MethodStatus status;
  if (OperatorImpl<SubOp>(context, *this, value, this, &status))
    return status;
  else
    return object()->SelfSub(context, value);
}

MethodStatus Value::SelfMul(Context* context, const Value& value) {
  MethodStatus status;
  if (OperatorImpl<MulOp>(context, *this, value, this, &status))
    return status;
  else
    return object()->SelfMul(context, value);
}

MethodStatus Value::SelfDiv(Context* context, const Value& value) {
  MethodStatus status;
  if (OperatorImpl<DivOp>(context, *this, value, this, &status))
    return status;
  else
    return object()->SelfDiv(context, value);
}

MethodStatus Value::SelfMod(Context* context, const Value& value) {
  MethodStatus status;
  if (IsReal() || value.IsReal()) {
    return MethodStatus::NewFail("mod operator doesn't work with real number");
  }
  if (OperatorImpl<ModOp>(context, *this, value, this, &status))
    return status;
  else
    return object()->SelfMod(context, value);
}

MethodStatus Value::Match(Context* context, const Value& value,
                          bool* output) const {
  if (IsObject())
    return object()->Match(context, value, output);
  else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s cannot work with \"~\" operator!", type_name());
  }
}

MethodStatus Value::NotMatch(Context* context, const Value& value,
                             bool* output) const {
  if (IsObject())
    return object()->NotMatch(context, value, output);
  else {
    return MethodStatus::NewUnimplemented(
        "Primitive type %s cannot work with \"!~\" operator!", type_name());
  }
}

MethodStatus Value::Unset(Context* context) {
  switch (m_type) {
    case TYPE_INTEGER:
      SetInteger(0);
      break;
    case TYPE_REAL:
      SetReal(0.0);
      break;
    case TYPE_BOOLEAN:
      SetBoolean(false);
      break;
    case TYPE_SIZE:
      SetSize(vcl::util::Size());
      break;
    case TYPE_DURATION:
      SetDuration(vcl::util::Duration());
      break;
    case TYPE_NULL:
      break;
    default:
      return object()->Unset(context);
  }
  return MethodStatus::kOk;
}

bool Value::ConvertToString(Context* context, const Value& value,
                            String** output) {
  switch (value.type()) {
    case TYPE_INTEGER:
      try {
        *output = context->gc()->NewString(
            boost::lexical_cast<std::string>(value.GetInteger()));
      } catch (...) {
        goto fail;
      }
      break;
    case TYPE_REAL:
      *output =
          context->gc()->NewString(vcl::util::RealToString(value.GetReal()));
      break;
    case TYPE_NULL:
      *output = context->gc()->NewString("null");
      break;
    case TYPE_BOOLEAN:
      *output = value.GetBoolean() ? context->gc()->NewString("true")
                                   : context->gc()->NewString("false");
      break;
    case TYPE_STRING:
      *output = value.GetString();
      break;
    case TYPE_DURATION:
      *output = (context->gc()->NewString(
          vcl::util::Duration::ToString(value.GetDuration())));
      break;
    case TYPE_SIZE:
      *output =
          context->gc()->NewString(vcl::util::Size::ToString(value.GetSize()));
      break;
    default:
      DCHECK(value.IsObject());
      {
        std::string str;
        MethodStatus result = value.object()->ToString(context, &str);
        if (result.is_ok()) {
          *output = context->gc()->NewString(str);
        } else {
          goto fail;
        }
      }
      break;
  }

  return true;

fail:
  return false;
}

bool Value::ConvertToInteger(Context* context, const Value& value,
                             int32_t* output) {
  switch (value.type()) {
    case TYPE_INTEGER:
      *output = value.GetInteger();
      break;
    case TYPE_REAL:
      *output = static_cast<int32_t>(value.GetReal());
      break;
    case TYPE_NULL:
      *output = 0;
      break;
    case TYPE_BOOLEAN:
      *output = value.GetBoolean() ? 1 : 0;
      break;
    case TYPE_STRING:
      try {
        *output = boost::lexical_cast<int32_t>(value.GetString()->data());
      } catch (...) {
        goto fail;
      }
    case TYPE_SIZE:
    case TYPE_DURATION:
      goto fail;
    default:
      DCHECK(value.IsObject());
      {
        MethodStatus result = value.object()->ToInteger(context, output);
        if (!result.is_ok()) {
          goto fail;
        }
      }
      break;
  }
  return true;

fail:
  return false;
}

bool Value::ConvertToReal(Context* context, const Value& value,
                          double* output) {
  switch (value.type()) {
    case TYPE_INTEGER:
      *output = static_cast<double>(value.GetInteger());
      break;
    case TYPE_REAL:
      *output = value.GetReal();
      break;
    case TYPE_NULL:
      *output = 0.0;
      break;
    case TYPE_BOOLEAN:
      *output = value.GetBoolean() ? 1.0 : 0.0;
      break;
    case TYPE_STRING:
      try {
        *output = boost::lexical_cast<double>(value.GetString()->data());
      } catch (...) {
        goto fail;
      }
    case TYPE_SIZE:
    case TYPE_DURATION:
      goto fail;
    default:
      DCHECK(value.IsObject());
      {
        MethodStatus result = value.GetObject()->ToReal(context, output);
        if (!result.is_ok()) {
          goto fail;
        }
      }
      break;
  }
  return true;

fail:
  return false;
}

bool Value::ConvertToBoolean(Context* context, const Value& value,
                             bool* output) {
  if (value.ToBoolean(context, output)) {
    return true;
  } else {
    return false;
  }
}

namespace {

typedef MethodStatus (Object::*CompareCallback)(Context*, const Value&,
                                                bool*) const;

struct LessOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left < right;
  }
};

struct LessEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left <= right;
  }
};

struct GreaterOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left > right;
  }
};

struct GreaterEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left >= right;
  }
};

struct EqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left == right;
  }
};

struct NotEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left != right;
  }
};

template <typename T>
MethodStatus ComparisonOp(Context* context, CompareCallback callback,
                          const Value& left, const Value& right, bool* output) {
  MethodStatus result;
  T handler;
  switch (left.type()) {
    case TYPE_INTEGER: {
      int32_t lhs = left.GetInteger();
      if (right.IsReal()) {
        *output = handler.Do(static_cast<double>(lhs), right.GetReal());
      } else {
        int32_t value;
        if (!(result = right.ToInteger(context, &value))) return result;
        *output = handler.Do(left.GetInteger(), value);
      }
      return MethodStatus::kOk;
    }
    case TYPE_REAL: {
      double value;
      if (!(result = right.ToReal(context, &value))) return result;
      *output = handler.Do(left.GetReal(), value);
      return MethodStatus::kOk;
    }
    case TYPE_NULL:
      if (boost::is_same<T, EqualOp>::value ||
          boost::is_same<T, NotEqualOp>::value) {
        *output = handler.Do(true, right.IsNull());
        return MethodStatus::kOk;
      } else {
        return MethodStatus::NewFail(
            "null only support comparison operator: "
            "==/!=");
      }
    case TYPE_BOOLEAN: {
      if (right.IsReal()) {
        *output = handler.Do(left.GetBoolean() ? 1.0 : 0.0, right.GetReal());
      } else {
        int32_t lhs = left.GetBoolean() ? 1 : 0;
        int32_t rhs;
        if (!(result = right.ToInteger(context, &rhs))) return result;
        *output = handler.Do(lhs, rhs);
      }
      return MethodStatus::kOk;
    }
    case TYPE_SIZE:
    case TYPE_DURATION:
      return MethodStatus::NewFail(
          "size and duration/time doesn't support "
          "comparison operator!");
    default:
      return (left.GetObject()->*callback)(context, right, output);
  }
}
}  // namespace

MethodStatus Value::Less(Context* context, const Value& value,
                         bool* output) const {
  return ComparisonOp<LessOp>(context, &Object::Less, *this, value, output);
}

MethodStatus Value::LessEqual(Context* context, const Value& value,
                              bool* output) const {
  return ComparisonOp<LessEqualOp>(context, &Object::LessEqual, *this, value,
                                   output);
}

MethodStatus Value::Greater(Context* context, const Value& value,
                            bool* output) const {
  return ComparisonOp<GreaterOp>(context, &Object::Greater, *this, value,
                                 output);
}

MethodStatus Value::GreaterEqual(Context* context, const Value& value,
                                 bool* output) const {
  return ComparisonOp<GreaterEqualOp>(context, &Object::GreaterEqual, *this,
                                      value, output);
}

MethodStatus Value::Equal(Context* context, const Value& value,
                          bool* output) const {
  return ComparisonOp<EqualOp>(context, &Object::Equal, *this, value, output);
}

MethodStatus Value::NotEqual(Context* context, const Value& value,
                             bool* output) const {
  return ComparisonOp<NotEqualOp>(context, &Object::NotEqual, *this, value,
                                  output);
}

MethodStatus Value::ToString(Context* context, std::string* output) const {
  switch (m_type) {
    case TYPE_INTEGER:
    case TYPE_REAL:
    case TYPE_NULL:
    case TYPE_SIZE:
    case TYPE_DURATION:
    case TYPE_BOOLEAN:
      return MethodStatus::NewFail("type %s cannot convert to string",
                                   type_name());
    default:
      return object()->ToString(context, output);
  }
}

MethodStatus Value::ToBoolean(Context* context, bool* output) const {
  switch (m_type) {
    case TYPE_INTEGER: {
      *output = GetInteger() ? true : false;
      return MethodStatus::kOk;
    }
    case TYPE_REAL: {
      *output = GetReal() ? true : false;
      return MethodStatus::kOk;
    }
    case TYPE_NULL:
      *output = false;
      return MethodStatus::kOk;
    case TYPE_SIZE:
    case TYPE_DURATION:
      *output = true;
      return MethodStatus::kOk;
    case TYPE_BOOLEAN:
      *output = GetBoolean();
      return MethodStatus::kOk;
    default:
      return object()->ToBoolean(context, output);
  }
}

MethodStatus Value::ToInteger(Context* context, int32_t* output) const {
  switch (m_type) {
    case TYPE_INTEGER:
      *output = GetInteger();
      return MethodStatus::kOk;
    case TYPE_REAL:
      *output = static_cast<int32_t>(GetReal());
      return MethodStatus::kOk;
    case TYPE_NULL:
    case TYPE_SIZE:
    case TYPE_DURATION:
      return MethodStatus::NewFail("type %s cannot convert to integer",
                                   type_name());
    case TYPE_BOOLEAN:
      *output = static_cast<int32_t>(GetBoolean());
      return MethodStatus::kOk;
    default:
      return object()->ToInteger(context, output);
  }
}

MethodStatus Value::ToReal(Context* context, double* output) const {
  switch (m_type) {
    case TYPE_INTEGER:
      *output = static_cast<double>(GetInteger());
      return MethodStatus::kOk;
    case TYPE_REAL:
      *output = GetReal();
      return MethodStatus::kOk;
    case TYPE_NULL:
    case TYPE_SIZE:
    case TYPE_DURATION:
      return MethodStatus::NewFail("type %s cannot convert to real",
                                   type_name());
    case TYPE_BOOLEAN:
      *output = static_cast<double>(GetBoolean());
      return MethodStatus::kOk;
    default:
      return object()->ToReal(context, output);
  }
}

MethodStatus Value::ToDisplay(Context* context, std::ostream* output) const {
  switch (m_type) {
    case TYPE_INTEGER:
      *output << "int(" << GetInteger() << ')';
      return MethodStatus::kOk;
    case TYPE_REAL:
      *output << "real(" << GetReal() << ')';
      return MethodStatus::kOk;
    case TYPE_NULL:
      *output << "null";
      return MethodStatus::kOk;
    case TYPE_BOOLEAN:
      *output << (GetBoolean() ? "true" : "false");
      return MethodStatus::kOk;
    case TYPE_SIZE:
      *output << GetSize();
      return MethodStatus::kOk;
    case TYPE_DURATION:
      *output << GetDuration();
      return MethodStatus::kOk;
    default:
      return object()->ToDisplay(context, output);
      return MethodStatus::kOk;
  }
}

MethodStatus Value::NewIterator(Context* context, Iterator** output) {
  if (IsObject()) {
    return object()->NewIterator(context, output);
  } else {
    return MethodStatus::NewFail("type %s doesn't support iterator",
                                 type_name());
  }
}

// ======================================================================
// Object Implementation
// ======================================================================
MethodStatus Object::GetProperty(Context* context, const String& key,
                                 Value* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(key);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "GetProperty not implement for type %s"
      "so cannot use \".\" operator!",
      type_name());
}

MethodStatus Object::SetProperty(Context* context, const String& key,
                                 const Value& output) {
  VCL_UNUSED(context);
  VCL_UNUSED(key);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "SetProperty not implement for type %s,"
      "so cannot use \".\" operator!",
      type_name());
}

MethodStatus Object::GetAttribute(Context* context, const String& key,
                                  Value* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(key);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "GetAttribute not implement for type %s"
      "so cannot use \":\" operator!",
      type_name());
}

MethodStatus Object::SetAttribute(Context* context, const String& key,
                                  const Value& output) {
  VCL_UNUSED(context);
  VCL_UNUSED(key);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "SetAttribute not implement for type %s"
      "so cannot use \":\" operator!",
      type_name());
}

MethodStatus Object::GetIndex(Context* context, const Value& index,
                              Value* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(index);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "GetIndex not implement for type %s"
      "so cannot use \"[]\" operator!",
      type_name());
}

MethodStatus Object::SetIndex(Context* context, const Value& index,
                              const Value& value) {
  VCL_UNUSED(context);
  VCL_UNUSED(index);
  VCL_UNUSED(value);
  return MethodStatus::NewUnimplemented(
      "SetIndex not implement for type %s"
      "so cannot use \"[]\" operator!",
      type_name());
}

MethodStatus Object::Invoke(Context* context, Value* output) {
  VCL_UNUSED(context);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "Invoke not implement for type %s"
      "so cannot invoke as function!",
      type_name());
}

#define DO(NAME, OP)                                                          \
  MethodStatus Object::NAME(Context* context, const Value& value,             \
                            Value* output) const {                            \
    VCL_UNUSED(context);                                                      \
    VCL_UNUSED(value);                                                        \
    VCL_UNUSED(output);                                                       \
    return MethodStatus::NewUnimplemented(#NAME                               \
                                          " not implement for type %s,"       \
                                          "so no support for operator \"" #OP \
                                          "\"!",                              \
                                          type_name());                       \
  }

DO(Add, +)
DO(Sub, -)
DO(Mul, *)
DO(Div, /)
DO(Mod, %)

#undef DO  // DO

#define DO(NAME, OP)                                                          \
  MethodStatus Object::NAME(Context* context, const Value& output) {          \
    VCL_UNUSED(context);                                                      \
    VCL_UNUSED(output);                                                       \
    return MethodStatus::NewUnimplemented(#NAME                               \
                                          " not implement for type %s,"       \
                                          "so no support for operator \"" #OP \
                                          "\"!",                              \
                                          type_name());                       \
  }

DO(SelfAdd, +=)
DO(SelfSub, -=)
DO(SelfMul, *=)
DO(SelfDiv, /=)
DO(SelfMod, %=)

#undef DO  // DO

MethodStatus Object::Unset(Context* context) {
  VCL_UNUSED(context);
  return MethodStatus::NewUnimplemented("Unset not implement for type %s",
                                        type_name());
}

#define DO(NAME, OP)                                                          \
  MethodStatus Object::NAME(Context* context, const Value& value,             \
                            bool* output) const {                             \
    VCL_UNUSED(context);                                                      \
    VCL_UNUSED(value);                                                        \
    VCL_UNUSED(output);                                                       \
    return MethodStatus::NewUnimplemented(#NAME                               \
                                          " not implement for type %s,"       \
                                          "so no support for operator \"" #OP \
                                          "\"!",                              \
                                          type_name());                       \
  }

DO(Less, <)
DO(LessEqual, <=)
DO(Greater, >)
DO(GreaterEqual, >=)

#undef DO  // DO

MethodStatus Object::Equal(Context* context, const Value& rhs,
                           bool* output) const {
  VCL_UNUSED(context);
  if (rhs.IsObject() && this == rhs.GetObject()) {
    *output = true;
  } else {
    *output = false;
  }
  return MethodStatus::kOk;
}

MethodStatus Object::NotEqual(Context* context, const Value& rhs,
                              bool* output) const {
  VCL_UNUSED(context);
  if (rhs.IsObject() && this == rhs.GetObject()) {
    *output = false;
  } else {
    *output = true;
  }
  return MethodStatus::kOk;
}

MethodStatus Object::Match(Context* context, const Value& value,
                           bool* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(value);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "Match not implement for type %s,"
      "so no support for operator \"~\"!",
      type_name());
}

MethodStatus Object::NotMatch(Context* context, const Value& value,
                              bool* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(value);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "NotMatch not implement for type %s,"
      "so no support for operator \"!~\"!",
      type_name());
}

MethodStatus Object::ToString(Context* context, std::string* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(output);
  return MethodStatus::NewFail("type %s cannot convert to string",
                               GetValueTypeName(m_type));
}

MethodStatus Object::ToBoolean(Context* context, bool* output) const {
  VCL_UNUSED(context);
  *output = true;
  return MethodStatus::kOk;
}

MethodStatus Object::ToInteger(Context* context, int32_t* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(output);
  return MethodStatus::NewFail("type %s cannot convert to integer",
                               GetValueTypeName(m_type));
}

MethodStatus Object::ToReal(Context* context, double* output) const {
  VCL_UNUSED(context);
  VCL_UNUSED(output);
  return MethodStatus::NewFail("type %s cannot convert to real",
                               GetValueTypeName(m_type));
}

MethodStatus Object::ToDisplay(Context* context, std::ostream* output) const {
  VCL_UNUSED(context);
  (*output) << "object(" << GetValueTypeName(m_type) << ")";
  return MethodStatus::kOk;
}

MethodStatus Object::NewIterator(Context* context, Iterator** output) {
  VCL_UNUSED(context);
  VCL_UNUSED(output);
  return MethodStatus::NewUnimplemented(
      "NewIterator not implemented for type %s,",
      "so no support for working with loop!", type_name());
}

Object::~Object() {}

// ========================================================================
// String Implementation
// ========================================================================

MethodStatus String::Add(Context* context, const Value& value,
                         Value* output) const {
  MethodStatus result;
  std::string temp;
  if (!(result = value.ToString(context, &temp))) return result;
  output->SetString(context->gc()->NewString(m_data + temp));
  return MethodStatus::kOk;
}

MethodStatus String::SelfAdd(Context* context, const Value& value) {
  MethodStatus result;
  std::string temp;
  if (!(result = value.ToString(context, &temp))) return result;
  m_data += temp;
  return MethodStatus::kOk;
}

MethodStatus String::Match(Context* context, const Value& value,
                           bool* output) const {
  MethodStatus result;
  if (value.IsString()) {
    String* str = value.GetString();
    if (!(result = str->Match(context, *this, output))) return result;
    return MethodStatus::kOk;
  } else {
    return MethodStatus::NewFail(
        "regex matching must applied on type string,but "
        "got type %s",
        value.type_name());
  }
}

namespace {

template <typename T>
MethodStatus StringCompareOp(Context* context, const String& left,
                             const Value& right, bool* output) {
  MethodStatus result;
  T handler;
  if (right.IsString()) {
    String* right_string = right.GetString();
    *output = handler.Do(left, *right_string);
    return MethodStatus::kOk;
  } else {
    std::string temp;
    if (!(result = right.ToString(context, &temp))) return result;
    *output = handler.Do(left.ToStdString(), temp);
    return MethodStatus::kOk;
  }
}

struct StringLessOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left < right;
  }
};

struct StringLessEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left <= right;
  }
};

struct StringGreaterOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left > right;
  }
};

struct StringGreaterEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left >= right;
  }
};

struct StringEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left == right;
  }
};

struct StringNotEqualOp {
  template <typename T>
  bool Do(const T& left, const T& right) const {
    return left != right;
  }
};

}  // namespace

MethodStatus String::Less(Context* context, const Value& value,
                          bool* output) const {
  return StringCompareOp<StringLessOp>(context, *this, value, output);
}

MethodStatus String::LessEqual(Context* context, const Value& value,
                               bool* output) const {
  return StringCompareOp<StringLessEqualOp>(context, *this, value, output);
}

MethodStatus String::Greater(Context* context, const Value& value,
                             bool* output) const {
  return StringCompareOp<StringGreaterOp>(context, *this, value, output);
}

MethodStatus String::GreaterEqual(Context* context, const Value& value,
                                  bool* output) const {
  return StringCompareOp<StringGreaterEqualOp>(context, *this, value, output);
}

MethodStatus String::Equal(Context* context, const Value& value,
                           bool* output) const {
  return StringCompareOp<StringEqualOp>(context, *this, value, output);
}

MethodStatus String::NotEqual(Context* context, const Value& value,
                              bool* output) const {
  return StringCompareOp<StringNotEqualOp>(context, *this, value, output);
}

MethodStatus String::ToString(Context* context, std::string* output) const {
  VCL_UNUSED(context);
  *output = m_data;
  return MethodStatus::kOk;
}

MethodStatus String::ToDisplay(Context* context, std::ostream* output) const {
  VCL_UNUSED(context);
  *output << "string(" << m_data << ")";
  return MethodStatus::kOk;
}

MethodStatus String::Regex::Match(Context* context, const String& pattern,
                                  const String& string, bool* output) {
  MethodStatus result;
  if (!ctx) {
    if (!(result = Init(context, pattern))) return result;
  }
  int stat[3];
  int ret = pcre_exec(ctx, extra, string.data(), string.size(), 0, 0, stat, 3);
  *output = (ret >= 0);
  return MethodStatus::kOk;
}

MethodStatus String::Regex::Init(Context* context, const String& pattern) {
  const char* error_str;
  int error_offset;
  ctx = pcre_compile(pattern.data(), 0, &error_str, &error_offset, NULL);
  if (!ctx)
    return MethodStatus::NewFail("cannot compile pattern %s due to : %s",
                                 pattern.data(), error_str);
  extra = pcre_study(ctx, 0, &error_str);
  return MethodStatus::kOk;
}

// ========================================================================
// List Implementation
// ========================================================================
MethodStatus List::NewIterator(Context* context, Iterator** iterator) {
  *iterator = context->gc()->New<ListIterator>(this);
  return MethodStatus::kOk;
}

MethodStatus List::GetIndex(Context* context, const Value& index,
                            Value* output) const {
  MethodStatus result;
  int32_t idx;
  if (!(result = index.ToInteger(context, &idx))) return result;
  if (idx < 0 || static_cast<size_t>(idx) >= m_list.size())
    return MethodStatus::NewFail("index out of range ,list size is:%zu",
                                 m_list.size());
  *output = m_list[idx];
  return MethodStatus::kOk;
}

MethodStatus List::SetIndex(Context* context, const Value& index,
                            const Value& value) {
  MethodStatus result;
  int32_t idx;
  if (!(result = index.ToInteger(context, &idx))) return result;
  if (idx < 0 || static_cast<size_t>(idx) >= m_list.size())
    return MethodStatus::NewFail("index out of range ,list size is:%zu",
                                 m_list.size());
  m_list[idx] = value;
  return MethodStatus::kOk;
}

MethodStatus List::ToDisplay(Context* context, std::ostream* output) const {
  (*output) << "list(";
  for (size_t i = 0; i < m_list.size(); ++i) {
    m_list[i].ToDisplay(context, output);
    (*output) << ',';
  }
  (*output) << ')';
  return MethodStatus::kOk;
}

void List::DoMark() {
  for (size_t i = 0; i < m_list.size(); ++i) {
    m_list[i].Mark();
  }
}

// =======================================================================
// Dict Implementation
// =======================================================================
MethodStatus Dict::NewIterator(Context* context, ::vcl::Iterator** iterator) {
  *iterator = context->gc()->New<DictIterator>(this);
  return MethodStatus::kOk;
}

MethodStatus Dict::GetProperty(Context* context, const String& key,
                               Value* output) const {
  VCL_UNUSED(context);
  const Value* result = m_dict.Find(key);
  if (result) {
    *output = *result;
    return MethodStatus::kOk;
  } else {
    return MethodStatus::NewFail("key \"%s\" not found", key.data());
  }
}

MethodStatus Dict::SetProperty(Context* context, const String& key,
                               const Value& value) {
  VCL_UNUSED(context);
  if (m_dict.size() >= kMaximumDictSize) {
    return MethodStatus::NewFail(
        "Cannot add more entry into dictionary,"
        "user can have a dictionary with no more "
        "than %zu entries",
        kMaximumDictSize);
  }
  m_dict.InsertOrUpdate(key, value);
  return MethodStatus::kOk;
}

MethodStatus Dict::GetIndex(Context* context, const Value& key,
                            Value* output) const {
  std::string k;
  if (!key.ToString(context, &k)) {
    return MethodStatus::NewFail(
        "type %s cannot be converted to string, which is "
        "required as a key for dictionary!",
        key.type_name());
  }
  const Value* rval = m_dict.Find(k);
  if (!rval) {
    return MethodStatus::NewFail("key \"%s\" not found", k.c_str());
  } else {
    *output = *rval;
    return MethodStatus::kOk;
  }
}

MethodStatus Dict::SetIndex(Context* context, const Value& key,
                            const Value& value) {
  std::string k;
  if (!key.ToString(context, &k)) {
    return MethodStatus::NewFail(
        "type %s cannot be converted to string, which is "
        "required as a key for dictionary!",
        key.type_name());
  }
  if (m_dict.size() >= kMaximumDictSize) {
    return MethodStatus::NewFail(
        "Cannot add more entry into dictionary,"
        "user can have a dictionary with no more "
        "than %zu entries",
        kMaximumDictSize);
  }
  m_dict.InsertOrUpdate(context->gc(), k, value);
  return MethodStatus::kOk;
}

MethodStatus Dict::ToDisplay(Context* context, std::ostream* output) const {
  StringDict<Value>::ConstIterator itr = m_dict.Begin();
  (*output) << "map(";
  for (; itr != m_dict.End(); ++itr) {
    itr->first->ToDisplay(context, output);
    (*output) << ':';
    itr->second.ToDisplay(context, output);
    (*output) << ",";
  }
  (*output) << ')';
  return MethodStatus::kOk;
}

// =======================================================================
// ACL Implementation
// =======================================================================
bool ACL::Match(const in_addr& addr) const { return m_impl->Match(addr); }

bool ACL::Match(const in6_addr& addr) const { return m_impl->Match(addr); }

bool ACL::Match(const char* addr) const { return m_impl->Match(addr); }

MethodStatus ACL::Match(Context* context, const Value& addr,
                        Value* output) const {
  MethodStatus result;
  std::string temp;
  if (!(result = addr.ToString(context, &temp)))
    return result;
  else {
    output->SetBoolean(Match(temp.c_str()));
    return MethodStatus::kOk;
  }
}

MethodStatus ACL::ToDisplay(Context* context, std::ostream* output) const {
  *output << "acl()";
  return MethodStatus::kOk;
}

ACL::~ACL() {}

ACL::ACL(vm::IPPattern* pattern) : Object(TYPE_ACL), m_impl(pattern) {}

// ========================================================================
// Extension Implementation
// ========================================================================
MethodStatus Extension::ToDisplay(Context* context,
                                  std::ostream* output) const {
  VCL_UNUSED(context);
  (*output) << "extension(" << type_name() << ')';
  return MethodStatus::kOk;
}

// ========================================================================
// Function Implementation
// ========================================================================
MethodStatus Function::ToDisplay(Context* context, std::ostream* output) const {
  VCL_UNUSED(context);
  *output << "function(" << m_name << ')';
  return MethodStatus::kOk;
}

// ========================================================================
// Action Implementation
// ========================================================================
MethodStatus Action::ToDisplay(Context* context, std::ostream* output) const {
  VCL_UNUSED(context);
  *output << "action(" << action_code_name();
  if (m_action_code == ACT_EXTENSION) {
    *output << ':' << m_extension_name;
  }
  *output << ')';
  return MethodStatus::kOk;
}

const char* Action::action_code_name() const {
#define __(A, B) \
  case A:        \
    return B;
  switch (m_action_code) {
    VCL_ACTION_LIST(__)
    default:
      return NULL;
  }
#undef __  // __
}

// ========================================================================
// Module Implementation
// ========================================================================
MethodStatus Module::ToDisplay(Context* context, std::ostream* output) const {
  *output << "module(" << m_name << "){";
  Map::ConstIterator itr = m_map.Begin();
  for (; itr != m_map.End(); ++itr) {
    *output << (itr->first)->data() << ':';
    itr->second.ToDisplay(context, output);
    *output << ",\n";
  }
  *output << '}';
  return MethodStatus::kOk;
}

MethodStatus Module::GetProperty(Context* context, const String& name,
                                 Value* output) const {
  VCL_UNUSED(context);
  if (FindProperty(name, output))
    return MethodStatus::kOk;
  else
    return MethodStatus::NewFail("key \"%s\" not found", name.data());
}

// ========================================================================
// SubRoutine Implementation
// ========================================================================
MethodStatus SubRoutine::ToDisplay(Context* context,
                                   std::ostream* output) const {
  VCL_UNUSED(context);
  *output << "sub(" << protocol() << ')';
  return MethodStatus::kOk;
}

const std::string& SubRoutine::name() const { return m_procedure->name(); }

const std::string& SubRoutine::protocol() const {
  return m_procedure->protocol();
}

size_t SubRoutine::argument_size() const {
  return m_procedure->argument_size();
}

SubRoutine::SubRoutine(vm::Procedure* procedure)
    : Object(TYPE_SUB_ROUTINE), m_procedure(procedure) {}

SubRoutine::~SubRoutine() {}

// ========================================================================
// GC
// ========================================================================
size_t GC::Collect() {
  Object* start = m_gc_start;
  Object** prev = &(m_gc_start);
  size_t collected = 0;

  // Swapping loop
  while (start) {
    Object* next;

    if (start->is_white()) {
      // When we reach here it means this object is not been scanned
      next = start->m_next;

      // Delete this object
      Delete(start);

      // Make the previous pointer points to the correct next
      *prev = next;

      // Bump counter for already collected object
      ++collected;
    } else {
      // When we reach here it means this object must have been visited
      // so keep it alive. The color of this object must be black
      DCHECK(start->is_black());

      // Reset the GC collection status
      start->set_gc_state(Object::GC_WHITE);

      // Change the previous pointer to this one
      prev = &(start->m_next);

      next = start->m_next;
    }

    // Move to next object in the chain
    start = next;
  }

  DCHECK(m_gc_size >= collected);
  Recalculate(collected);
  m_gc_size -= collected;
  return collected;
}

GC::~GC() {
  size_t count = 0;
  while (m_gc_start) {
    Object* next = m_gc_start->m_next;
    Delete(m_gc_start);
    m_gc_start = next;
    ++count;
  }
  DCHECK(count == m_gc_size);
}

void GC::Dump(std::ostream* output) const {
  Object* obj = m_gc_start;
  size_t i = 0;
  while (obj) {
    switch (obj->type()) {
      case TYPE_STRING:
        *output << i << ". str(" << static_cast<String*>(obj)->data() << ")\n";
        break;
      case TYPE_LIST:
        *output << i << ". " << '[' << static_cast<List*>(obj)->size() << "]\n";
        break;
      case TYPE_DICT:
        *output << i << ". " << '{' << static_cast<Dict*>(obj)->size() << "}\n";
        break;
      case TYPE_ACL:
        *output << i << ". "
                << "ACL\n";
        break;
      case TYPE_FUNCTION:
        *output << i << ". func(" << static_cast<Function*>(obj)->name()
                << ")\n";
        break;
      case TYPE_EXTENSION:
        *output << i << ". ext("
                << static_cast<Extension*>(obj)->extension_name() << ")\n";
        break;
      case TYPE_ACTION:
        *output << i << ". act("
                << static_cast<Action*>(obj)->action_code_name() << ")\n";
        break;
      case TYPE_MODULE:
        *output << i << ". mod(" << static_cast<Module*>(obj)->name() << ")\n";
        break;
      case TYPE_SUB_ROUTINE:
        *output << i << ". sub(" << static_cast<SubRoutine*>(obj)->name()
                << ")\n";
        break;
      default:
        VCL_UNREACHABLE();
    }
    ++i;
    obj = obj->m_next;
  }
}

// ========================================================================
// ContextGC Implementation
// ========================================================================
void ContextGC::Mark() {
  // Mark the root
  for (RootNodeList::iterator itr = m_root_list.begin();
       itr != m_root_list.end(); ++itr) {
    // Here we need to call DoMark instead of Mark function since
    // Mark doesn't take care of coloring and is used for user to
    // extend our GC capabilities.
    (*itr).object->Mark();
  }

  // Mark the context
  if (m_context) m_context->Mark();
}

// ========================================================================
// Context Implementation
// ========================================================================
void Context::Mark() {
  Base::Mark();
  m_runtime->Mark();
}

Context::Context(const ContextOption& opt,
                 const boost::shared_ptr<CompiledCode>& cc)
    : m_runtime(),
      m_compiled_code(cc),
      m_gc(opt.gc_trigger, opt.gc_ratio, this) {
  m_runtime.reset(new vm::Runtime(this, opt.max_calling_stack_size));
}

Context::~Context() {}

MethodStatus Context::Construct() {
  Value result;
  SubRoutine* e =
      InternalAllocator(&m_gc).NewSubRoutine(m_compiled_code->entry());
  AddOrUpdateGlobalVariable(kEntryProcName, Value(e));
  return Invoke(e, &result);
}

// Implementation for Invoke function inside of Context
MethodStatus Context::Invoke(SubRoutine* sub_routine,
                             const std::vector<Value>& argument,
                             Value* output) {
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) {
    return result;
  }
  for (size_t i = 0; i < argument.size(); ++i)
    m_runtime->AddArgument(argument[i]);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, Value* output) {
  DCHECK(sub_routine->argument_size() == 0);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             Value* output) {
  DCHECK(sub_routine->argument_size() == 1);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, Value* output) {
  DCHECK(sub_routine->argument_size() == 2);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, const Value& a3, Value* output) {
  DCHECK(sub_routine->argument_size() == 3);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  m_runtime->AddArgument(a3);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, const Value& a3, const Value& a4,
                             Value* output) {
  DCHECK(sub_routine->argument_size() == 4);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  m_runtime->AddArgument(a3);
  m_runtime->AddArgument(a4);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, const Value& a3, const Value& a4,
                             const Value& a5, Value* output) {
  DCHECK(sub_routine->argument_size() == 5);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  m_runtime->AddArgument(a3);
  m_runtime->AddArgument(a4);
  m_runtime->AddArgument(a5);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, const Value& a3, const Value& a4,
                             const Value& a5, const Value& a6, Value* output) {
  DCHECK(sub_routine->argument_size() == 6);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  m_runtime->AddArgument(a3);
  m_runtime->AddArgument(a4);
  m_runtime->AddArgument(a5);
  m_runtime->AddArgument(a6);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, const Value& a3, const Value& a4,
                             const Value& a5, const Value& a6, const Value& a7,
                             Value* output) {
  DCHECK(sub_routine->argument_size() == 7);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  m_runtime->AddArgument(a3);
  m_runtime->AddArgument(a4);
  m_runtime->AddArgument(a5);
  m_runtime->AddArgument(a6);
  m_runtime->AddArgument(a7);
  return m_runtime->FinishRun(sub_routine, output);
}

MethodStatus Context::Invoke(SubRoutine* sub_routine, const Value& a1,
                             const Value& a2, const Value& a3, const Value& a4,
                             const Value& a5, const Value& a6, const Value& a7,
                             const Value& a8, Value* output) {
  DCHECK(sub_routine->argument_size() == 8);
  MethodStatus result;
  if (!(result = m_runtime->BeginRun(sub_routine))) return result;
  m_runtime->AddArgument(a1);
  m_runtime->AddArgument(a2);
  m_runtime->AddArgument(a3);
  m_runtime->AddArgument(a4);
  m_runtime->AddArgument(a5);
  m_runtime->AddArgument(a6);
  m_runtime->AddArgument(a7);
  m_runtime->AddArgument(a8);
  return m_runtime->FinishRun(sub_routine, output);
}

size_t Context::GetArgumentSize() const { return m_runtime->GetArgumentSize(); }

Value Context::GetArgument(size_t index) const {
  return m_runtime->GetArgument(index);
}

bool Context::Yield() { return m_runtime->Yield(); }

bool Context::is_yield() const { return m_runtime->is_yield(); }

MethodStatus Context::Resume(Value* output) {
  return m_runtime->Resume(output);
}

// ==========================================================================
// CompiledCode
// ==========================================================================
CompiledCode::CompiledCode(Engine* engine)
    : m_source_code_list(),
      m_sub_routine_list(),
      m_entry(NULL),
      m_engine(engine),
      m_gc() {
  m_entry = InternalAllocator(&m_gc).NewEntryProcedure();
  m_sub_routine_list.push_back(m_entry);
}

CompiledCode::~CompiledCode() {}

uint32_t CompiledCode::AddSourceCodeInfo(
    const boost::shared_ptr<SourceCodeInfo>& sci) {
  for (size_t i = 0; i < m_source_code_list.size(); ++i) {
    const boost::shared_ptr<SourceCodeInfo>& info = m_source_code_list[i];
    if (info->file_path == sci->file_path) return static_cast<uint32_t>(i);
  }
  m_source_code_list.push_back(sci);
  return static_cast<uint32_t>(m_source_code_list.size() - 1);
}

boost::shared_ptr<SourceCodeInfo> CompiledCode::IndexSourceCodeInfo(
    uint32_t index) const {
  if (index < m_source_code_list.size())
    return m_source_code_list[index];
  else
    return boost::shared_ptr<SourceCodeInfo>();
}

void CompiledCode::Dump(std::ostream& output) const {
  for (size_t i = 0; i < m_sub_routine_list.size(); ++i) {
    m_sub_routine_list[i].Dump(output);
    output << '\n';
  }
}

// ==========================================================================
// Engine
// ==========================================================================
Engine::Engine() : Base(), m_gc() { AddBuiltin(this); }

boost::shared_ptr<CompiledCode> Engine::LoadFile(const std::string& filename,
                                                 const ScriptOption& option,
                                                 std::string* error) {
  std::string source_code;
  if (!vcl::util::LoadFile(filename, &source_code)) {
    error->assign("cannot open source file!");
    return boost::shared_ptr<CompiledCode>();
  }
  return LoadString(filename, source_code, option, error);
}

boost::shared_ptr<CompiledCode> Engine::LoadString(
    const std::string& filename, const std::string& source_code,
    const ScriptOption& option, std::string* error) {
  boost::shared_ptr<CompiledCode> cc(new CompiledCode(this));
  SourceRepo source_repo(NULL, option.allow_loop);
  vm::CompilationUnit cu;

  // 1. Initialize source code repo which will be used to hold all *parsed*
  // file and cache them while we are doing compilation.
  if (!source_repo.Initialize(filename, source_code, error)) {
    return boost::shared_ptr<CompiledCode>();
  }

  // 2. Generate compilation unit. This phase will do all include expansion
  // and load all required source code and parse them
  if (!vm::CompilationUnit::Generate(&cu, cc.get(), &source_repo,
                                     option.max_include, option.folder_hint,
                                     option.allow_absolute_path, error)) {
    return boost::shared_ptr<CompiledCode>();
  }

  // 3. Do the compilation
  if (!vm::Compile(cc.get(), source_repo.zone(), cu, error)) {
    return boost::shared_ptr<CompiledCode>();
  }

  return cc;
}

static double kVCLVersion = 4.0;

bool CheckVCLVersion(double version) { return version == kVCLVersion; }

void InitVCL(const char* process_path, double version) {
  kVCLVersion = version;
  google::InitGoogleLogging(process_path);
}

}  // namespace vcl
