#include "target-lua51.h"
#include "template.h"

#include "../ast.h"
#include "../zone.h"
#include "../compilation-unit.h"
#include "../vcl-pri.h"
#include "../parser.h"

#include <vcl/util.h>
#include <string>

#include <boost/range.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace vcl {
namespace vm  {
namespace transpiler {
namespace lua51 {
namespace {
class Transpiler;

#ifdef VCL_TRANSPILER_INDENT
static const char*  kIndent     = VCL_TRNASPILER_INDENT;
static const size_t kIndentSize = ((sizeof(VCL_TRNASPILER_INDENT) / sizeof(char)) -1);
#else
static const char*  kIndent     = "  ";
static const size_t kIndentSize = 2;
#endif // VCL_TRANSPILER_INDENT


// The commented out keyword are keywords that are shared between Lua5.1 and VCL
static const char* kLua51KeyWord[] = {
  "and",
//  "break",
  "do",
//  "else",
//  "elseif",
  "end",
//  "false",
//  "for",
  "function",
//  "if",
  "in",
  "local",
  "nil",
  "not",
  "or",
  "repeat",
//  "return",
  "then",
//  "true",
  "until",
  "while"
};


inline std::string GetIndent( int indent ) {
  std::string buffer;
  buffer.reserve( kIndentSize * indent );
  for( int i = 0 ; i < indent ; ++i ) {
    buffer.append(kIndent);
  }
  return buffer;
}

std::string GetCurrentTime() {
  namespace pt = boost::posix_time;
  return pt::to_iso_string( pt::second_clock::local_time() );
}

class Comment {
 public:
  Comment( std::string* output , int indent ):
    m_output(output),
    m_indent(indent)
  {}

  inline Comment& Line(const char* , ...) ;
  inline Comment& Line(const std::string& path,const char* , const ::vcl::util::CodeLocation& );
  inline Comment& Line(const std::string& path,const ::vcl::util::CodeLocation& , const char* , ... );
 private:
  std::string* m_output;
  int m_indent;
};

inline Comment& Comment::Line(const char* format,...) {
  va_list vl;
  va_start(vl,format);
  m_output->append(GetIndent(m_indent));
  m_output->append("-- ");
  m_output->append(::vcl::util::FormatV(format,vl));
  m_output->push_back('\n');
  return *this;
}

inline Comment& Comment::Line( const std::string& path , const ::vcl::util::CodeLocation& loc ,
    const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  return Line(path,::vcl::util::FormatV(format,vl).c_str(),loc);
}

inline Comment& Comment::Line( const std::string& path , const char* message,
    const ::vcl::util::CodeLocation& loc ) {
  m_output->append(GetIndent(m_indent));
  m_output->append("-- ");
  m_output->append("$vcl(message=\"");
  m_output->append(message);
  m_output->append(::vcl::util::Format("\";location=%zu,%zu,%zu;path=\"",
                                         loc.line,
                                         loc.position,
                                         loc.ccount));
  m_output->append(path);
  m_output->append("\")\n");
  return *this;
}

class Collection {
 public:
  inline Collection( std::string* output , const char* start , const char* end ):
    m_output(output),
    m_end(end)
  { if(start) m_output->append(start); }

  ~Collection() { if(m_end) m_output->append(m_end); }

  void Add( const std::string& left , const std::string& right , bool comma ) const {
    m_output->append(left);
    m_output->append(" = ");
    m_output->append(right);
    if(comma) m_output->push_back(',');
  }

  void Add( const std::string& value , bool comma ) const
  { m_output->append(value); if(comma) m_output->push_back(','); }

 private:
  std::string* m_output;
  const char* m_end;
};

inline void FormatAppend( std::string* buffer , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  buffer->append( ::vcl::util::FormatV(format,vl) );
}

class Transpiler {
 public:
  Transpiler( const std::string& filename ,
              const Options& opt ,
              const CompiledCode& cc,
              std::string* output ,
              std::string* error ) :
      m_filename(filename),
      m_opt(opt),
      m_cc(cc),
      m_output(output),
      m_error(error),
      m_te(),
      m_source_index()
  {}

  bool DoTranspile( const CompilationUnit& cu );

 private: // Textual helper functions
  void ReportError( const ::vcl::util::CodeLocation& , const char* , ... );
  void WriteLine( int indent , const char* , ... );
  void WriteLine( std::string* , int indent ,  const char* , ... );
  void WriteTemplateLine( int indent , const char* , Template::Argument& );
  void WriteTemplateLine( std::string* , int indent , const char* , Template::Argument& );

  bool GenerateFunctionPrototype( const std::string& key ,
                                  std::string* output ,
                                  const ast::Sub& node,
                                  int indent );

  bool GenerateFunctionArg( const std::string& key ,
                            std::string* output ,
                            const ast::FuncCall& node ,
                            int indent );

  bool GenerateExntesionInitializer( const std::string& key ,
                                     std::string* output ,
                                     const ast::ExtensionInitializer& node ,
                                     int indent );

  Template::Generator NewFunctionPrototype( const ast::Sub& node, int indent ) {
    return boost::bind(&Transpiler::GenerateFunctionPrototype,this,_1,_2,
        boost::ref(node),indent);
  }

  Template::Generator NewFunctionArgGenerator( const ast::FuncCall& node , int indent ) {
    return boost::bind(&Transpiler::GenerateFunctionArg,this,_1,_2,
        boost::ref(node),indent);
  }

  Template::Generator NewExtensionInitializerGenerator(
      const ast::ExtensionInitializer& node , int indent ) {
    return boost::bind(&Transpiler::GenerateExntesionInitializer,this,_1,_2,
        boost::ref(node),indent);
  }

  std::string EscapeLuaString( const char* ) const;

  const std::string& CurrentSourceFile() const {
    return m_cc.IndexSourceCodeInfo(m_source_index)->file_path;
  }

  bool CheckIdentifierName( const vcl::util::CodeLocation& , zone::ZoneString* );

 private:
  // Initialization
  void SetupHeader();
  void SetupFooter();

  bool Transpile( const CompilationUnit::SubList&);
  bool Transpile( const CompilationUnit& cu );

  bool Transpile( const ast::Import& , int indent );
  bool Transpile( const ast::Global& , int indent );
  bool Transpile( const ast::Extension& , int indent );

  bool Transpile( const ast::Sub& , int indent , std::string* output );
  bool Transpile( const ast::AST& , int indent , std::string* output );
  bool TranspileChunk( const ast::Chunk& , int indent , std::string* output );
  bool Transpile( const ast::Stmt& , int indent , std::string* output );
  bool Transpile( const ast::Declare& , int indent , std::string* output );
  bool Transpile( const ast::Set& , int indent , std::string* output );
  bool Transpile( const ast::Unset& , int indent , std::string* output );
  bool Transpile( const ast::Return& , int indent , std::string* output );
  bool Transpile( const ast::Terminate& , int indent , std::string* output );
  bool Transpile( const ast::If& , int indent , std::string* output );
  bool Transpile( const ast::LexScope& , int indent , std::string* output );
  bool TranspileCallStatement( const ast::FuncCall&, int indent , std::string* output );

  bool TranspileMethodCall( const ast::FuncCall& , int indent , std::string* );
  bool TranspileFuncCall( const ast::FuncCall& , int indent , std::string* );
  bool TranspileExpression( const ast::AST& , int indent , std::string* );

  bool TranspileAnonymouosSub( const ast::Sub& , int indent , std::string* );
  bool Transpile( const ast::ExtensionLiteral& , int indent , std::string* );
  bool Transpile( const ast::List& , int indent , std::string* );
  bool Transpile( const ast::Dict& , int indent , std::string* );
  bool Transpile( const ast::Size& , int indent , std::string* );
  bool Transpile( const ast::Duration& , int indent , std::string* );
  bool Transpile( const ast::String& , int indent , std::string* );
  bool Transpile( const ast::Variable&, int indent , std::string* );
  bool Transpile( const ast::Null& , int indent , std::string* );
  bool Transpile( const ast::Boolean& , int indent , std::string* );
  bool Transpile( const ast::Integer& , int indent , std::string* );
  bool Transpile( const ast::Real& , int indent , std::string* );
  bool Transpile( const ast::StringInterpolation& , int indent , std::string* );
  bool Transpile( const ast::StringConcat& , int indent , std::string* );
  bool Transpile( const ast::Unary& , int indent , std::string* );
  bool Transpile( const ast::Binary& , int indent , std::string* );
  bool Transpile( const ast::Ternary& , int indent , std::string* );
  bool Transpile( const ast::Prefix&  , size_t target , int indent , std::string* );

 private:
  const std::string& m_filename;
  const Options& m_opt;
  const CompiledCode& m_cc;
  std::string* m_output;
  std::string* m_error;
  Template m_te;

  uint32_t m_source_index;
};

void Transpiler::ReportError( const ::vcl::util::CodeLocation& loc ,
    const char* format , ... ) {
  va_list vlist;
  va_start(vlist,format);
  *m_error = ::vcl::util::ReportErrorV(
      m_cc.IndexSourceCodeInfo(m_source_index)->source_code,
      loc,
      "transpiler",
      format,
      vlist);
}

void Transpiler::WriteLine( std::string* output , int indent ,
    const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  output->append(GetIndent(indent));
  output->append(::vcl::util::FormatV(format,vl));
  output->push_back('\n');
}

void Transpiler::WriteLine( int indent , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  m_output->append(GetIndent(indent));
  m_output->append(::vcl::util::FormatV(format,vl));
  m_output->push_back('\n');
}

void Transpiler::WriteTemplateLine( int indent , const char* format ,
    Template::Argument& arg ) {
  m_output->append(GetIndent(indent));
  CHECK( m_te.Render(format,arg,m_output) );
  m_output->push_back('\n');
}

void Transpiler::WriteTemplateLine( std::string* output , int indent ,
    const char* format , Template::Argument& arg ) {
  output->append(GetIndent(indent));
  CHECK( m_te.Render(format,arg,output) );
  output->push_back('\n');
}

bool Transpiler::CheckIdentifierName( const vcl::util::CodeLocation& loc , zone::ZoneString* name ) {
  std::string n(name->ToStdString());
  if(n.find(m_opt.temporary_variable_prefix) == 0)
    goto fail;

  if(n == m_opt.vcl_main_coroutine ||
     n == m_opt.vcl_main ||
     n == m_opt.vcl_terminate_code ||
     n == m_opt.vcl_type_name ||
     n == m_opt.vcl_add_function_name ||
     n == m_opt.inline_module_name ||
     n == m_opt.runtime_namespace)
    goto fail;

  for( size_t i = 0 ; i < boost::size(kLua51KeyWord) ; ++i ) {
    if(n == kLua51KeyWord[i])
      goto fail;
  }

  return true;

fail:
  ReportError(loc,"Cannot use identifier name %s which collide with builtin variable "
                  "name or Lua5.1 keyword. Please change your variable name !",
                  name->data());
  return false;
}

bool Transpiler::GenerateFunctionPrototype( const std::string& key ,
                                            std::string* output ,
                                            const ast::Sub& node ,
                                            int indent ) {
  VCL_UNUSED(key);
  VCL_UNUSED(indent);

  std::string arg_buffer;
  Collection wrapper(output,NULL,NULL);
  for( size_t i = 0 ; i < node.arg_list.size() ; ++i ) {
    wrapper.Add( node.arg_list.Index(i)->data() , (i < node.arg_list.size()-1) );
  }
  return true;
}

bool Transpiler::GenerateFunctionArg( const std::string& key ,
                                      std::string* output ,
                                      const ast::FuncCall& node ,
                                      int indent ) {
  VCL_UNUSED(key);
  VCL_UNUSED(indent);

  std::string arg_buffer;
  Collection wrapper(output,NULL,NULL);

  for( size_t i = 0; i < node.argument.size() ; ++i ) {
    const ast::AST& a = *node.argument.Index(i);
    arg_buffer.clear();
    if(!TranspileExpression(a,indent,&arg_buffer)) return false;
    wrapper.Add(arg_buffer,(i < node.argument.size()-1));
  }
  return true;
}

bool Transpiler::GenerateExntesionInitializer( const std::string& key ,
                                               std::string* output ,
                                               const ast::ExtensionInitializer& node ,
                                               int indent ) {
  VCL_UNUSED(key);
  VCL_UNUSED(indent);

  std::string arg_buffer;
  Collection wrapper(output , "{" ,"}");

  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    const ast::ExtensionInitializer::ExtensionField& f = node.list.Index(i);
    arg_buffer.clear();
    if(!TranspileExpression(*f.value,indent,&arg_buffer)) return false;
    wrapper.Add(f.name->data(),arg_buffer,(i < node.list.size()-1));
  }

  return true;
}

std::string Transpiler::EscapeLuaString( const char* str ) const {
  std::string buffer;
  buffer.reserve(512);

  for( ; *str ; ++str ) {
    switch(*str) {
      case '\a': buffer.push_back('\\') ; buffer.push_back('a'); break;
      case '\b': buffer.push_back('\\') ; buffer.push_back('b'); break;
      case '\f': buffer.push_back('\\') ; buffer.push_back('f'); break;
      case '\n': buffer.push_back('\\') ; buffer.push_back('n'); break;
      case '\r': buffer.push_back('\\') ; buffer.push_back('r'); break;
      case '\t': buffer.push_back('\\') ; buffer.push_back('t'); break;
      case '\v': buffer.push_back('\\') ; buffer.push_back('v'); break;
      case '\\': buffer.push_back('\\') ; buffer.push_back('\\');break;
      case '\"': buffer.push_back('\\') ; buffer.push_back('"'); break;
      default: buffer.push_back(*str); break;
    }
  }

  return buffer;
}

void Transpiler::SetupHeader() {
  Comment comment(m_output,0);
  comment.Line("*********************************************************************************")
         .Line("************ This file is generated automatically, DO NOT MODIFY ****************")
         .Line("************ Source : %s **************", m_filename.c_str())
         .Line("************ Comment: %s **************", m_opt.comment.c_str())
         .Line("************ Time: %s *****************", GetCurrentTime().c_str())
         .Line("*********************************************************************************");

  //  Setup the global variable for us to handle terminate return code

  comment.Line("builtin VCL variable for terminating return");
  WriteLine(0,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.empty_code);

  comment.Line("*************************** Builtin Functions Start *****************************");
  if( m_opt.allow_builtin_add ) {
    WriteLine(0,"function %s(a,b) \n%sreturn "
                "((type(a) == \"string\" and type(b) ==\"string\") and (a..b) or (a+b))\nend",
                m_opt.vcl_add_function_name.c_str(),kIndent);
  }
  comment.Line("*************************** Builtin Functions End  ******************************");
}

void Transpiler::SetupFooter() {
  Comment comment(m_output,0);
  comment.Line("*********************************************************************************")
         .Line("******************** Generated by VCL transpiler ********************************")
         .Line("*********************************************************************************");
}

/* -----------------------------------------------------------------------
 *
 * Expression Level
 *
 * ---------------------------------------------------------------------*/

bool Transpiler::TranspileAnonymouosSub( const ast::Sub& node , int indent , std::string* output ) {
  // 1. Generate function prototype
  {
    Template::Argument arg;
    arg["arg"] = Template::Value( NewFunctionPrototype(node,indent) );
    WriteTemplateLine(output,0,"function(${arg})",arg);
  }

  // 2. Generate the function body
  if(!Transpile(node,indent+1,output))
    return false;

  // 3. end the function
  output->append("end\n");
  return true;
}

bool Transpiler::Transpile( const ast::ExtensionLiteral& node , int indent , std::string* output ) {
  Template::Argument arg;
  arg["ns"] = Template::Str(m_opt.runtime_namespace);
  arg["name"] = Template::Str(node.type_name->data());
  arg["arg"] = Template::Value(NewExtensionInitializerGenerator(*node.initializer,indent));

  CHECK( m_te.Render("${ns}.extension.${name}( ${arg} )",arg,output) );
  return true;
}

bool Transpiler::Transpile( const ast::List& node , int indent , std::string* output ) {
  Collection wrapper(output,"{","}");
  std::string arg_buffer;
  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    arg_buffer.clear();
    if(!TranspileExpression(*node.list.Index(i),indent,&arg_buffer)) return false;

    // We cannot use Lua's builtin array , but have to generate key value pair for Lua
    // to force it use 0 start index otherwise all the semantic gonna break. This is
    // OK unless we don't use too much loop code here. If we will need to generate code
    // for looping, the best bet we can have is using something like this:
    //
    //   for i,0,#array then
    //     print(array[i])
    //   end
    //
    // ipairs will not work in this case
    wrapper.Add(::vcl::util::Format("[%zu]",i),arg_buffer,true);
  }

  // Add a tag into the table to show this is a list
  wrapper.Add(m_opt.vcl_type_name,"\"list\"",false);
  return true;
}

bool Transpiler::Transpile( const ast::Dict& node , int indent , std::string* output ) {
  Collection wrapper(output,"{","}");
  std::string arg_buffer;
  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    const ast::Dict::Entry& e = node.list[i];
    // Generate key
    output->push_back('[');
    arg_buffer.clear();
    if(!TranspileExpression(*e.key,indent,&arg_buffer)) return false;
    output->append(arg_buffer);
    output->push_back(']');

    // Generate assign
    output->append(" = " );

    // Generate value
    arg_buffer.clear();
    if(!TranspileExpression(*e.value,indent,&arg_buffer)) return false;
    output->append(arg_buffer);

    if( i < node.list.size() - 1 ) output->push_back(',');
  }

  return true;
}

bool Transpiler::Transpile( const ast::Size& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);

  Template::Argument arg;
  arg["ns"] = Template::Str( m_opt.runtime_namespace );
  arg["b"]  = Template::Str( ::vcl::util::Format("%d",node.value.bytes) );
  arg["kb"] = Template::Str( ::vcl::util::Format("%d",node.value.kilobytes) );
  arg["mb"] = Template::Str( ::vcl::util::Format("%d",node.value.megabytes) );
  arg["gb"] = Template::Str( ::vcl::util::Format("%d",node.value.gigabytes) );

  CHECK( m_te.Render("%{ns}.new_size({b=${b},kb=${kb},mb=${mb},gb=${gb}})",arg,output) );
  return true;
}

bool Transpiler::Transpile( const ast::Duration& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);

  Template::Argument arg;
  arg["ns"] = Template::Str( m_opt.runtime_namespace );
  arg["hour"]= Template::Str( ::vcl::util::Format("%d",node.value.hour) );
  arg["minute"] = Template::Str( ::vcl::util::Format("%d",node.value.minute) );
  arg["second"] = Template::Str( ::vcl::util::Format("%d",node.value.second) );
  arg["millisecond"] = Template::Str( ::vcl::util::Format("%d",node.value.millisecond) );

  CHECK( m_te.Render("%{ns}.new_duration({"
                       "hour=${hour},"
                       "minute=${minute},"
                       "second=${second},"
                       "millisecond=${millisecond}})",arg,output));
  return true;
}

bool Transpiler::Transpile( const ast::String& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  output->push_back('"');
  output->append( EscapeLuaString(node.value->data()) );
  output->push_back('"');
  return true;
}

bool Transpiler::Transpile( const ast::Variable& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  if(!CheckIdentifierName(node.location,node.value))
    return false;
  output->append( node.value->data() );
  return true;
}

bool Transpiler::Transpile( const ast::Null& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  VCL_UNUSED(node);

  output->append("nil");
  return true;
}

bool Transpiler::Transpile( const ast::Boolean& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  output->append( node.value ? "true" : "false" );
  return true;
}

bool Transpiler::Transpile( const ast::Integer& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  output->append( ::vcl::util::Format( "%d", node.value ) );
  return true;
}

bool Transpiler::Transpile( const ast::Real& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  output->append( ::vcl::util::Format( "%f", node.value ) );
  return true;
}

bool Transpiler::Transpile( const ast::StringInterpolation& node, int indent , std::string* output ) {
  VCL_UNUSED(indent);
  std::string arg_buffer;

  // Efficiently handle string concatenation in most *managed* environment
  // include using a string buffer like structure otherwise too many
  // temporary objects will be created which adds extra GC overheads in VM.
  //
  // In Lua the simplest way is to use a table and then call table.concate

  output->append("table.concate({");

  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    output->append("tostring(");
    arg_buffer.clear();
    if(!TranspileExpression(*node.list.Index(i),indent,&arg_buffer)) return false;
    output->append(arg_buffer);
    output->push_back(')');
    if(i < node.list.size()-1) output->append(",");
  }

  output->append("},\"\")");

  return true;
}

bool Transpiler::Transpile( const ast::StringConcat& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  std::string buffer;
  buffer.reserve(1024);

  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    buffer.append( node.list.Index(i)->data() );
  }

  output->push_back('"');
  output->append( EscapeLuaString(buffer.c_str()) );
  output->push_back('"');
  return true;
}

bool Transpiler::Transpile( const ast::Unary& node , int indent , std::string* output ) {
  size_t sub_count = 0;

  for( size_t i = 0 ; i < node.ops.size() ; ++i ) {
    switch(node.ops.Index(i)) {
      case TK_ADD:
        break;
      case TK_SUB:
        output->push_back('-');
        ++sub_count;
        if(sub_count == 2) {
          ReportError(node.location,
                      "Cannot put 2 consecutive \"-\" to serve as unary operator,"
                      "this is allowed in VCL virtual machine but Lua will treat it "
                      "as comment!");
          return false;
        }
        break;

      case TK_NOT:
        output->append("not");
        sub_count = 0;
        break;
      default:
        VCL_UNREACHABLE();
        return false;
    }
  }

  if(!TranspileExpression(*node.operand,indent,output)) return false;
  return true;
}

bool Transpiler::Transpile( const ast::Binary& node , int indent , std::string* output ) {
  if(node.op == TK_MATCH || node.op == TK_NOT_MATCH) {
    std::string lhs_buffer , rhs_buffer;
    if(!TranspileExpression(*node.lhs,indent,&lhs_buffer) ||
       !TranspileExpression(*node.rhs,indent,&rhs_buffer))
      return false;

    if(node.op == TK_MATCH) {
      FormatAppend(output,"%s.match(%s,%s)",m_opt.runtime_namespace.c_str(),
                                            lhs_buffer.c_str(),
                                            rhs_buffer.c_str());
    } else {
      FormatAppend(output,"%s.not_match(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                lhs_buffer.c_str(),
                                                rhs_buffer.c_str());
    }
  } else if(node.op == TK_ADD) {
    // To support polymorphic add , which Lua doesn't have , we need to emit all
    // operator '+' as a function call and inside of the function to simulate that
    // polymorphic add.
    std::string lhs_buffer , rhs_buffer;
    if(!TranspileExpression(*node.lhs,indent,&lhs_buffer) ||
       !TranspileExpression(*node.rhs,indent,&rhs_buffer))
      return false;

    FormatAppend(output,"%s(%s,%s)",m_opt.vcl_add_function_name.c_str(),
                                    lhs_buffer.c_str(),
                                    rhs_buffer.c_str());
  } else {
    output->push_back('(');
    if(!TranspileExpression(*node.lhs,indent,output)) return false;
    switch(node.op) {
      case TK_SUB: output->append(" - "); break;
      case TK_MUL: output->append(" * "); break;
      case TK_DIV: output->append(" / "); break;
      case TK_MOD: output->append(" % "); break;
      case TK_LT:  output->append(" < "); break;
      case TK_LE:  output->append("<=") ; break;
      case TK_GT:  output->append(" > "); break;
      case TK_GE:  output->append(" >= ");break;
      case TK_EQ:  output->append(" == ");break;
      case TK_NE:  output->append(" ~= ");break;
      case TK_AND: output->append(" and "); break;
      case TK_OR:  output->append(" or " ); break;
      default: VCL_UNREACHABLE(); break;
    }

    if(!TranspileExpression(*node.rhs,indent,output)) return false;
    output->push_back(')');
  }
  return true;
}

bool Transpiler::Transpile( const ast::Ternary& node , int indent , std::string* output ) {
  output->push_back('(');
  if(!TranspileExpression(*node.condition,indent,output)) return false;
  output->append(" and ");
  if(!TranspileExpression(*node.first,indent,output)) return false;
  output->append(" or ");
  if(!TranspileExpression(*node.second,indent,output)) return false;
  output->push_back(')');
  return true;
}

bool Transpiler::Transpile( const ast::Prefix& node , size_t target , int indent ,
    std::string* output ) {
  std::string temp;

  target = target == 0 ? node.list.size() : target;


  // Check the leading variable to this prefix expression is not identifier
  // that is builtin
  DCHECK( node.list.First().tag == ast::Prefix::Component::DOT );
  if(!CheckIdentifierName( node.location , node.list.First().var ))
    return false;

  for( size_t i = 0 ; i < target ; ++i ) {
    const ast::Prefix::Component& n = node.list.Index(i);
    switch(n.tag) {
      case ast::Prefix::Component::CALL:
        if(!TranspileFuncCall(*n.funccall,indent,&temp)) return false;
        break;
      case ast::Prefix::Component::INDEX:
        temp.push_back('[');
        if(!TranspileExpression(*n.expression,indent,&temp)) return false;
        temp.push_back(']');
        break;
      case ast::Prefix::Component::DOT:
        if( i > 0 ) temp.push_back('.');
        temp.append(n.var->data());
        break;
      case ast::Prefix::Component::ATTRIBUTE:
        {
          std::string buffer;
          // Now we need to wrap the temporary buffer's stuff as a function call into a
          // {runtime}.get_attr(object,key) call since Lua doesn't support attribute syntax.
          FormatAppend(&buffer,"%s.get_attr(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                    temp.c_str(),
                                                    n.var->data());
          temp.swap(buffer);
          break;
        }
      case ast::Prefix::Component::MCALL:
        temp.push_back(':');
        if(!TranspileMethodCall(*n.funccall,indent,&temp)) return false;
        break;
      default:
        VCL_UNREACHABLE();
        break;
    }
  }
  output->append(temp);
  return true;
}

bool Transpiler::TranspileExpression( const ast::AST& expr , int indent , std::string* output ) {
  switch(expr.type) {
    case ast::AST_TERNARY:
      return Transpile( static_cast<const ast::Ternary&>(expr) , indent , output );
    case ast::AST_BINARY:
      return Transpile( static_cast<const ast::Binary&>(expr), indent, output );
    case ast::AST_UNARY:
      return Transpile( static_cast<const ast::Unary&>(expr), indent, output );
    case ast::AST_PREFIX:
      return Transpile( static_cast<const ast::Prefix&>(expr), 0 , indent, output );
    case ast::AST_FUNCCALL:
      return TranspileFuncCall( static_cast<const ast::FuncCall&>(expr), indent, output );
    case ast::AST_EXTENSION_LITERAL:
      return Transpile( static_cast<const ast::ExtensionLiteral&>(expr), indent, output );
    case ast::AST_DICT:
      return Transpile( static_cast<const ast::Dict&>(expr),indent, output );
    case ast::AST_STRING_CONCAT:
      return Transpile( static_cast<const ast::StringConcat&>(expr), indent, output);
    case ast::AST_INTEGER:
      return Transpile( static_cast<const ast::Integer&>(expr), indent, output);
    case ast::AST_REAL:
      return Transpile( static_cast<const ast::Real&>(expr), indent, output);
    case ast::AST_BOOLEAN:
      return Transpile( static_cast<const ast::Boolean&>(expr), indent, output);
    case ast::AST_NULL:
      return Transpile( static_cast<const ast::Null&>(expr), indent, output);
    case ast::AST_STRING:
      return Transpile( static_cast<const ast::String&>(expr), indent, output);
    case ast::AST_VARIABLE:
      return Transpile( static_cast<const ast::Variable&>(expr), indent, output);
    case ast::AST_DURATION:
      return Transpile( static_cast<const ast::Duration&>(expr), indent, output);
    case ast::AST_LIST:
      return Transpile( static_cast<const ast::List&>(expr), indent, output);
    case ast::AST_SIZE:
      return Transpile( static_cast<const ast::Size&>(expr), indent, output);
    case ast::AST_STRING_INTERPOLATION:
      return Transpile( static_cast<const ast::StringInterpolation&>(expr), indent, output);
    case ast::AST_SUB:
      return TranspileAnonymouosSub(static_cast<const ast::Sub&>(expr),indent,output);
    default:
      VCL_UNREACHABLE();
      return false;
  }
}

/* -------------------------------------------------------------------------------
 *
 * Statement
 *
 * ------------------------------------------------------------------------------*/

bool Transpiler::Transpile( const ast::Stmt& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"prefix_call",node.location);
  output->append(GetIndent(indent));
  if(!TranspileExpression(*node.expr,indent,output))
    return false;
  output->push_back('\n');
  return true;
}

bool Transpiler::Transpile( const ast::Declare& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"declare",node.location);
  std::string buffer;
  if(!CheckIdentifierName(node.location,node.variable))
    return false;
  if(!TranspileExpression(*node.rhs,indent,&buffer))
    return false;
  WriteLine( output , indent , "local %s = %s" , node.variable->data(), buffer.c_str() );
  return true;
}

bool Transpiler::Transpile( const ast::Set& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"set",node.location);

  std::string buffer;
  if(!TranspileExpression(*node.rhs,indent,&buffer)) return false;

  if( node.lhs.type == ast::LeftHandSide::VARIABLE ) {
    if(!CheckIdentifierName(node.location,node.lhs.variable)) return false;

    WriteLine( output , indent , "%s = %s", node.lhs.variable->data(),
                                   buffer.c_str());
  } else {
    std::string object;

    const ast::Prefix::Component& last_comp = node.lhs.prefix->list.Last();

    // Check if we need to use set_attr to set the last attribute
    if( last_comp.tag == ast::Prefix::Component::ATTRIBUTE ) {

      // Transpile the component in the access chain up to the last component
      if(!Transpile(*node.lhs.prefix,node.lhs.prefix->list.size()-1,indent,&object))
        return false;

      // Handle the last component
      WriteLine( output , indent , "%s.set_attr( %s , %s , %s )", m_opt.runtime_namespace.c_str(),
                                                                  object.c_str(),
                                                                  last_comp.var->data(),
                                                                  buffer.c_str() );
    } else {
      if(!Transpile(*node.lhs.prefix,0,indent,&object))
        return false;

      WriteLine( output , indent , "%s = %s", object.c_str() , buffer.c_str());
    }
  }
  return true;
}

bool Transpiler::Transpile( const ast::Unset& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"unset",node.location);

  // The unset is a little bit complicated because the way the unset work
  // is kind of anti-intuitive for Lua.
  if( node.lhs.type == ast::LeftHandSide::VARIABLE ) {
    // Check the identifier name for this unset operation
    if(!CheckIdentifierName(node.location,node.lhs.variable)) return false;

    // Due to the fact all primitive cannot be passed based on reference
    // so we have to generate unset code *inplace* for that variable to
    // make the unset actually work

    Template::Argument arg;

    arg["i1"] = Template::Str(GetIndent(indent));
    arg["i2"] = Template::Str(GetIndent(indent+1));
    arg["lb"] = Template::Str("\n");
    arg["obj"]= Template::Str(node.lhs.variable->data());

    CHECK( m_te.Render( "${i1}if type(${obj}) == \"string\" then${lb}"
                        "${i2}${obj} = \"\"${lb}"
                        "${i1}elseif type(${obj}) == \"number\" then${lb}"
                        "${i2}${obj} = 0${lb}"
                        "${i1}elseif type(${obj}) == \"boolean\" then${lb}"
                        "${i2}${obj} = false${lb}"
                        "${i1}else${lb}"
                        "${i2}${obj} = nil${lb}"
                        "${i1}end${lb}" , arg, output) );
  } else {
    const ast::Prefix::Component& last_comp = node.lhs.prefix->list.Last();
    std::string buffer;

    if(!Transpile(*node.lhs.prefix,node.lhs.prefix->list.size()-1,indent,&buffer))
      return false;

    switch(last_comp.tag) {
      case ast::Prefix::Component::ATTRIBUTE:
        WriteLine(output,indent,"%s.unset_attr(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                       buffer.c_str(),
                                                       last_comp.var->data());
        break;
      case ast::Prefix::Component::INDEX:
        {
          std::string expr;
          if(!TranspileExpression(*last_comp.expression,indent,&expr))
            return false;
          WriteLine(output,indent,"%s.unset_prop(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                         buffer.c_str(),
                                                         expr.c_str());
        }
        break;
      case ast::Prefix::Component::DOT:
        WriteLine(output,indent,"%s.unset_prop(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                       buffer.c_str(),
                                                       last_comp.var->data());
        break;
      default:
        VCL_UNREACHABLE();
        break;
    }
  }
  return true;
}

bool Transpiler::Transpile( const ast::Return& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"return",node.location);

  output->append( GetIndent(indent) );
  output->append( "return ");
  if(!node.value) {
    output->append("nil");
    return true;
  } else {
    if(!TranspileExpression(*node.value,indent,output)) return false;
    output->push_back('\n');
    return true;
  }
}

bool Transpiler::Transpile( const ast::Terminate& node, int indent , std::string* output ) {
  if( !m_opt.allow_terminate_return ) {
    ReportError(node.location,"Terminate style return is disallowed!");
    return false;
  }

  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"terminate",node.location);

  switch(node.action) {
    case ACT_OK:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.ok_code);
      break;
    case ACT_FAIL:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.fail_code);
      break;
    case ACT_PIPE:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.pipe_code);
      break;
    case ACT_HASH:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.hash_code);
      break;
    case ACT_PURGE:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.purge_code);
      break;
    case ACT_LOOKUP:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.lookup_code);
      break;
    case ACT_RESTART:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.restart_code);
      break;
    case ACT_FETCH:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.fetch_code);
      break;
    case ACT_MISS:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.miss_code);
      break;
    case ACT_DELIVER:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.deliver_code);
      break;
    case ACT_RETRY:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.retry_code);
      break;
    case ACT_ABANDON:
      WriteLine(output, indent,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.abandon_code);
      break;
    default:
      ReportError(node.location,"Unsupport terminated return!");
      break;
  }

  WriteLine(indent,"coroutine.yield()");
  return true;
}

bool Transpiler::Transpile( const ast::If& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"if",node.location);

  std::string condition;

  { // Leading if
    const ast::If::Branch& n = node.branch_list.First();
    if(!TranspileExpression(*n.condition,indent,&condition))
      return false;
    WriteLine(output, indent,"if %s then",condition.c_str());
    if(!TranspileChunk( *n.body , indent + 1 , output )) return false;
  }

  // Rest of elseif or else
  for( size_t i = 1 ; i < node.branch_list.size() ; ++i ) {
    const ast::If::Branch& n = node.branch_list.Index(i);
    if(n.condition) {
      condition.clear();
      if(!TranspileExpression(*n.condition,indent,&condition))
        return false;
      WriteLine(output, indent,"elseif %s then",condition.c_str());
      if(!TranspileChunk(*n.body,indent+1,output)) return false;
    } else {
      WriteLine(output,indent,"else");
      if(!TranspileChunk(*n.body,indent+1,output)) return false;
    }
  }

  WriteLine(indent,"end");
  return true;
}

bool Transpiler::Transpile( const ast::LexScope& node , int indent , std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),"scope",node.location);

  WriteLine(output,indent,"do");
  if(!TranspileChunk(*node.body,indent+1,output)) return false;
  WriteLine(output,indent,"end");
  return true;
}

bool Transpiler::Transpile( const ast::Import& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"import",node.location);

  // NOTES:
  //
  // we still need to check the *module name* against the builtin variable identifier
  // since the import will result in a new variable being created and we should not
  // have module uses existed builtin name.
  if(!CheckIdentifierName(node.location,node.module_name))
    return false;

  if( m_opt.allow_module_inline ) {
    WriteLine(indent,"local %s = %s.%s", node.module_name->data(),
                                         m_opt.inline_module_name.c_str(),
                                         node.module_name->data());
  } else {
    WriteLine(indent,"local %s = require(\"%s\")",node.module_name->data(),
                                                  node.module_name->data());
  }
  return true;
}

bool Transpiler::Transpile( const ast::Global& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"global",node.location);

  if(!CheckIdentifierName(node.location,node.name))
    return false;

  std::string buffer;
  if(!TranspileExpression(*node.value,indent,&buffer))
    return false;

  WriteLine(indent,"%s = %s",node.name->data(),buffer.c_str());
  return true;
}

bool Transpiler::Transpile( const ast::Extension& node, int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"extension",node.location);

  Template::Argument arg;
  arg["ns"] = Template::Str(m_opt.runtime_namespace);
  arg["name"] = Template::Str(node.type_name->data());
  arg["arg"] = Template::Value(NewExtensionInitializerGenerator(
        *node.initializer,indent));
  arg["obj"] = Template::Str(node.instance_name->data());
  WriteTemplateLine(indent,"${obj} = ${ns}.extension.${name}(${arg})",arg);
  return true;
}

bool Transpiler::Transpile( const ast::Sub& node , int indent , std::string* output ) {
  return TranspileChunk(*node.body,indent,output);
}


bool Transpiler::Transpile( const ast::AST& node , int indent , std::string* output ) {
  switch(node.type) {
    case ast::AST_TERMINATE:
      return Transpile( static_cast<const ast::Terminate&>(node) , indent , output );
    case ast::AST_RETURN:
      return Transpile( static_cast<const ast::Return&>(node), indent , output );
    case ast::AST_SET:
      return Transpile( static_cast<const ast::Set&>(node), indent , output );
    case ast::AST_UNSET:
      return Transpile( static_cast<const ast::Unset&>(node), indent , output );
    case ast::AST_DECLARE:
      return Transpile( static_cast<const ast::Declare&>(node), indent , output );
    case ast::AST_IF:
      return Transpile( static_cast<const ast::If&>(node) , indent , output );
    case ast::AST_STMT:
      return Transpile( static_cast<const ast::Stmt&>(node), indent , output );
    case ast::AST_FUNCCALL:
      return TranspileCallStatement(static_cast<const ast::FuncCall&>(node),indent, output);
    case ast::AST_LEXSCOPE:
      return Transpile( static_cast<const ast::LexScope&>(node), indent, output);
    default:
      ReportError(node.location,"Statement: %s doesn't support in transpilation!",
                                GetASTName( node.type ));
      return false;
  }
}

bool Transpiler::TranspileChunk( const ast::Chunk& node , int indent ,
    std::string* output ) {
  for( size_t i = 0 ; i < node.statement_list.size() ; ++i ) {
    if(!Transpile(*node.statement_list.Index(i),indent,output))
      return false;
  }

  return true;
}

bool Transpiler::Transpile( const CompilationUnit::SubList& node ) {
  const ast::Sub& first_sub = *node.front().sub;

  Comment comment(m_output,0);
  comment.Line(CurrentSourceFile(),first_sub.location,
                                   "sub(%s)",
                                   first_sub.sub_name->data());

  if(!CheckIdentifierName(first_sub.location,first_sub.sub_name)) return false;

  // Generate sub's argument list
  {
    Template::Argument arg;
    arg["arg"] = Template::Value( NewFunctionPrototype(*node.front().sub,0) );
    arg["name"]= Template::Str( node.front().sub->sub_name->data() );
    WriteTemplateLine(0,"function ${name}(${arg})",arg);
  }

  // Generate body of each sub
  {
    for( size_t i = 0 ; i < node.size() ; ++i ) {
      const CompilationUnit::SubStatement& ss = node[i];
      if( m_source_index != ss.source_index ) {
        m_source_index = ss.source_index;
      }

      Comment comment(m_output,1);
      comment.Line(CurrentSourceFile(),ss.sub->location,"sub(%s)",
                                                        node.front().sub->sub_name->data());

      WriteLine(1,"do");
      if(!Transpile(*ss.sub,2,m_output)) return false;
      WriteLine(1,"end");
    }
  }

  // Generate the end of the function
  WriteLine(0,"end");
  return true;
}

bool Transpiler::Transpile( const CompilationUnit& cu ) {
  if(!cu.empty()) {
    m_source_index = cu.Index(0).source_index;
    for( size_t i = 0 ; i < cu.size() ; ++i ) {
      const CompilationUnit::Statement& stmt = cu.Index(i);
      if(m_source_index != stmt.source_index) {
        m_source_index = stmt.source_index;
      }

      switch(stmt.code.which()) {
        case CompilationUnit::STMT_AST:
          {
            // All statment here can only be global statement
            const ast::AST& node = *boost::get<const ast::AST*>(stmt.code);
            switch(node.type) {
              case ast::AST_IMPORT:
                if(!Transpile(static_cast<const ast::Import&>(node),0))
                  return false;
                break;
              case ast::AST_EXTENSION:
                if(!Transpile(static_cast<const ast::Extension&>(node),0))
                  return false;
                break;
              case ast::AST_DECLARE:
                // Method support will generate temporary local variable
                if(!Transpile(static_cast<const ast::Declare&>(node),0,m_output))
                  return false;
                break;
              case ast::AST_GLOBAL:
                if(!Transpile(static_cast<const ast::Global&>(node),0))
                  return false;
                break;
              default:
                VCL_UNREACHABLE();
                break;
            }
          }
          break;
        case CompilationUnit::STMT_SUBLIST:
          if(!Transpile(*boost::get<CompilationUnit::SubListPtr>(stmt.code)))
            return false;
          break;
        default: VCL_UNREACHABLE(); break;
      }
    }
  }
  return true;
}

bool Transpiler::TranspileMethodCall( const ast::FuncCall& node , int indent ,
    std::string* output ) {
  return TranspileFuncCall(node,indent,output);
}

bool Transpiler::TranspileFuncCall( const ast::FuncCall& node , int indent ,
    std::string* output ) {
  // Transpile function call as a normal call expression but not as a statement
  // basically do not consider indent in such case
  Template::Argument arg;
  arg["arg"] = Template::Value( NewFunctionArgGenerator(node,indent) );
  if(node.name) {
    arg["name"]= Template::Str( node.name->data() );
    CHECK( m_te.Render("${name}(${arg})",arg,output) );
  } else {
    CHECK( m_te.Render("(${arg})",arg,output) );
  }
  return true;
}

bool Transpiler::TranspileCallStatement( const ast::FuncCall& node , int indent ,
    std::string* output ) {
  Comment comment(output,indent);
  comment.Line(CurrentSourceFile(),node.location,"call(%s)",node.name->data());

  Template::Argument arg;
  arg["arg"] = Template::Value( NewFunctionArgGenerator(node,indent) );
  arg["name"]= Template::Str(node.name->data());
  WriteTemplateLine(output,indent,"${name}(${arg})",arg);
  return true;
}

bool Transpiler::DoTranspile( const CompilationUnit& cu ) {
  SetupHeader();
  if(!Transpile(cu)) return false;
  SetupFooter();
  return true;
}

} // namespace

bool Transpile( const std::string& filename , const CompiledCode& cc ,
                const CompilationUnit& cu, const Options& option,
                std::string* output , std::string* error ) {
  Transpiler instance(filename,option,cc,output,error);
  return instance.DoTranspile(cu);
}

namespace {

template< typename T >
bool MaybeSet( const ::vcl::experiment::TranspilerOptionTable& tt ,
               const std::string& field,
               T* output ,
               std::string* error ) {
  typename ::vcl::experiment::TranspilerOptionTable::const_iterator
    itr = tt.find(field);

  if( itr == tt.end() )
    return true;
  else {
    const ::vcl::experiment::TranspilerOptionValue& v = itr->second;
    if(!v.Get(output)) {
      *error = ::vcl::util::Format("lua51 transpiler option %s type mismatch!",
                                   field.c_str());
      return false;
    }
    return true;
  }
}

} // namespace

bool Options::Create( const ::vcl::experiment::TranspilerOptionTable& tt ,
                      Options* opt ,
                      std::string* error ) {
#define DO(XX,YY) if(!MaybeSet(tt,XX,&(opt->YY),error)) return false

  DO("comment",comment);
  DO("temporary_variable_prefix",temporary_variable_prefix);
  DO("vcl_main",vcl_main);
  DO("vcl_main_coroutine",vcl_main_coroutine);
  DO("vcl_terminate_code",vcl_terminate_code);
  DO("vcl_type_name",vcl_type_name);
  DO("vcl_add_function_name",vcl_add_function_name);
  DO("allow_builtin_add",allow_builtin_add);
  DO("allow_terminate_return",allow_terminate_return);
  DO("ok_code",ok_code);
  DO("fail_code",fail_code);
  DO("pipe_code",pipe_code);
  DO("hash_code",hash_code);
  DO("purge_code",purge_code);
  DO("lookup_code",lookup_code);
  DO("restart_code",restart_code);
  DO("fetch_code",fetch_code);
  DO("miss_code",miss_code);
  DO("deliver_code",deliver_code);
  DO("retry_code",retry_code);
  DO("abandon_code",abandon_code);
  DO("empty_code",empty_code);
  DO("allow_module_inline",allow_module_inline);
  if(opt->allow_module_inline) DO("inline_module_name",inline_module_name);
  DO("runtime_namespace",runtime_namespace);

#undef DO // DO

  return true;
}


} // namespace lua51
} // namespace transpiler
} // namespace vm
} // namespace vcl
