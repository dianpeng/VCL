#include "target-lua51.h"
#include "template.h"
#include <vcl/util.h>
#include <string>

#include <boost/bind.hpp>

namespace vcl {
namespace vm  {
namespace transpiler {
namespace lua51 {

namespace {

class Transpiler;

class Comment {
 public:
  inline Comment( std::string* , int indent );
  Comment& Line(const std::string& path,const char* , ...) const;
  Comment& Line(const char* , const vcl::util::CodeLocation& loc);
  Comment& Line(const std::string& path,const vcl::util::CodeLocation& , const char* , ... );
  Comment& Raw (const char* , ...) const;
 private:
  std::string* m_output;
  int m_indent;
};

class Liner {
 public:
  inline Liner( std::string* , int indent , const char* , ... );
  inline Liner( std::string* , int indent );
  Liner& Write(const char* , ...) const;
  inline ~Liner();
 private:
  std::string* m_output;
  int m_indent;
};

class Collection {
 public:
  inline Collection( std::string* output , const char* prefix ):
    m_output(output),
    m_prefix(prefix)
  { if(prefix) m_output->append(prefix); }

  ~Collection() { if(m_prefix) m_output->append(m_prefix); }

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
  const char* m_prefix;
};

void FormatAppend( std::string* buffer , const char* format , ... );

class Transpiler {
 public:
  Transpiler( zone::Zone* zone ,
              const std::string& filename ,
              const std::string& comment ,
              const Options& opt ,
              const CompilationUnit& cu ,
              const CompiledCode& cc,
              std::string* output ,
              std::string* error )
    : m_zone(zone),
      m_filename(filename),
      m_comment(comment),
      m_opt(opt),
      m_cu(cu),
      m_cc(cc),
      m_output(output),
      m_error(error),
      m_te(),
      m_source_index()
  {}

  bool DoTranspile();

 private: // Textual helper functions
  void ReportError( const vcl::util::CodeLocation& , const char* , ... );
  std::string GetIndent( int indent ) const;
  void WriteLine( int indent , const char* , ... );
  void WriteTemplateLine( int indent , const char* , Template::Argument& argument );

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
  Template::Generator NewFunctionPrototype( const std::string& key ,
                                            std::string* output,
                                            const ast::Sub& node,
                                            int indent ) {
    return boost::bind(&Transpiler::GenerateFunctionPrototype,this,_1,_2,node,indent);
  }

  Template::Generator NewFunctionArgGenerator( const ast::FuncCall& node , int indent ) {
    return boost::bind(&Transpiler::GenerateFunctionArg,this,_1,_2,node,indent);
  }

  Template::Generator NewExtensionInitializerGenerator(
      const ast::ExtensionInitializer& node , int indent ) {
    return boost::bind(&Transpiler::GenerateExntesionInitializer,this,_1,_2,node,indent);
  }

  std::string EscapeLuaString( const char* ) const;

  const std::string& CurrentSourceFile() const {
    return m_cc.IndexSourceCodeInfo(m_source_index).file_path;
  }

 private:
  // Initialization
  void SetupHeader();
  void SetupFooter();

  bool Transpile( const CompilationUnit::SubList&);
  bool Transpile( const CompilationUnit& cu );
  bool Transpile( const ast::AST& , int indent );
  bool TranspileChunk( const ast::Chunk& , int indent );

  bool Transpile( const ast::Import& , int indent );
  bool Transpile( const ast::Global& , int indent );
  bool Transpile( const ast::Extension& , int indent );
  bool Transpile( const ast::Sub& , int indent );
  bool TranspileAnonymousSub(const ast::Sub& , int indent );

  bool Transpile( const ast::Stmt& , int indent );
  bool Transpile( const ast::Declare& , int indent );
  bool Transpile( const ast::Set& , int indent );
  bool Transpile( const ast::Unset& , int indent );
  bool Transpile( const ast::Return& , int indent );
  bool Transpile( const ast::Terminate& , int indent );
  bool Transpile( const ast::If& , int indent );
  bool Transpile( const ast::LexScope& , int indent );

  bool TranspileExpression( const ast::AST* , int indent , std::string* );
  bool Transpile( const ast::ExtensionLiteral& , int indent , std::string* );
  bool Transpile( const ast::List& , int indent , std::string* );
  bool Transpile( const ast::Dict& , int indent , std::string* );
  bool Transpile( const ast::Size& , int indent , std::string* );
  bool Transpile( const ast::Duration& , int indent , std::string* );
  bool Transpile( const ast::String& , int indent , std::string* );
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

  // Misc
  bool Transpile( const ast::FuncCall& node , int indent , std::string* );

 private:
  zone::Zone* m_zone;
  const std::string& m_filename;
  const std::string& m_comment;
  const Options& m_opt;
  const CompilationUnit& m_cu;
  const CompiledCode& m_cc;
  std::string* m_output;
  std::string* m_error;
  Template m_te;

  int m_source_index;
};


bool Transpiler::GenerateFunctionPrototype( const std::string& key ,
                                            std::string* output ,
                                            const ast::Sub& node ,
                                            int indent ) {
  VCL_UNUSED(key);

  std::string arg_buffer;
  Collection wrapper(output,NULL);
  for( size_t i = 0 ; i < node.arg_list.size() ; ++i ) {
    wrapper.Add( node.arg_list.Index(i)->data() , (i < node.argument.size()-1) );
  }
  return true;
}

bool Transpiler::GenerateFunctionArg( const std::string& key ,
                                      std::string* output ,
                                      const ast::FuncCall& node ,
                                      int indent ) {
  VCL_UNUSED(key);
  std::string arg_buffer;
  Collection wrapper(output,NULL);

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
  std::string arg_buffer;
  Collection wrapper(output , "{" );

  for( size_t i = 0 ; i < node.argument.size() ; ++i ) {
    const ExtensionInitializer::ExtensionField& f = node.argument.Index(i);
    arg_buffer.clear();
    if(!TranspileExpression(*f.value,indent,&arg_buffer)) return false;
    wrapper.Add(f.name->data(),arg_buffer,(i < node.argument.size()-1));
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
         .Line("************ Source : %s ************* " , m_filename.c_str() )
         .Line("************ Comment: %s ************* " , m_comment.c_str()  )
         .Line("*********************************************************************************");

  //  Setup the global variable for us to handle terminate return code
  WriteLine(0,"%s = %d",m_opt.vcl_terminate_code.c_str(),m_opt.empty_code);
}


/* -----------------------------------------------------------------------
 *
 * Expression Level
 *
 * ---------------------------------------------------------------------*/
bool Transpiler::Transpile( const ast::ExtensionLiteral& node , int indent , std::string* output ) {
  Template::Argument arg;
  arg["ns"] = Template::Str(m_opt.runtime_namespace);
  arg["name"] = Template::Str(node.type_name->data());
  arg["arg"] = Template::Value(NewExtensionInitializerGenerator(node,indent));

  CHECK( m_te.DoRender("${ns}.extension.${name}( ${arg} )",arg,output) );
  return true;
}

bool Transpiler::Transpile( const ast::List& node , int indent , std::string* output ) {
  Collection wrapper(output,"{");
  std::string arg_buffer;
  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    arg_buffer.clear();
    if(!TranspileExpression(*node.list.Index(i),&arg_buffer,indent)) return false;

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
    wrapper.Add(vcl::util::Format("[%zu]",i),arg_buffer,true);
  }

  // Add a tag into the table to show this is a list
  wrapper.Add(m_opt.vcl_type_name,"list",false);
  return true;
}

bool Transpiler::Transpile( const ast::Dict& node , int indent , std::string* output ) {
  Collection wrapper(output,"{");
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
    if(!TranspileExpression(*e.value,&arg_buffer)) return false;
    output->append(arg_buffer);

    if( i < node.list.size() - 1 ) output->push_back(',');
  }

  return true;
}

bool Transpiler::Transpile( const ast::Size& node , int indent , std::string* output ) {
  Template::Argument arg;
  arg["ns"] = Template::Str( m_opt.runtime_namespace );
  arg["b"]  = Template::Str( vcl::util::Format("%d",node.value.b) );
  arg["kb"] = Template::Str( vcl::util::Format("%d",node.value.kb) );
  arg["mb"] = Template::Str( vcl::util::Format("%d",node.value.mb) );
  arg["gb"] = Template::Str( vcl::util::Format("%d",node.value.gb) );

  CHECK( m_te.DoRender("%{ns}.new_size({b=${b},kb=${kb},mb=${mb},gb=${gb}})",arg,output) );
  return true;
}

bool Transpiler::Transpile( const ast::Duration& node , int indent , std::string* output ) {
  Template::Argument arg;
  arg["ns"] = Template::Str( m_opt.runtime_namespace );
  arg["hour"]= Template::Str( vcl::util::Format("%d",node.value.hour) );
  arg["minute"] = Template::Str( vcl::util::Format("%d",node.value.minute) );
  arg["second"] = Template::Str( vcl::util::Format("%d",node.value.second) );
  arg["millisecond"] = Template::Str( vcl::util::Format("%d",node.value.millisecond) );

  CHECK( m_te.DoRender("%{ns}.new_duration({"
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

bool Transpiler::Transpile( const ast::Null& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
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
  output->append( vcl::util::Format( "%d", node.value ) );
  return true;
}

bool Transpiler::Transpile( const ast::Real& node , int indent , std::string* output ) {
  VCL_UNUSED(indent);
  output->append( vcl::util::Format( "%f", node.value ) );
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

  output->push_back("table.concate({");

  for( size_t i = 0 ; i < node.list.size() ; ++i ) {
    output->append("tostring(");
    arg_buffer.clear();
    if(!Transpile(*node.list.Index(i),indent,&arg_buffer)) return false;
    output->append(arg_buffer);
    output->push_back(')');
    if(i < node.list.sizez()-1) output->append(",");
  }

  output->push_back("},\"\")");

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

  if(!Transpile(*node.operand,indent,output)) return false;
  return true;
}

bool Transpiler::Transpile( const ast::Binary& node , int indent , std::string* output ) {
  if(node.op == TK_MATCH || node.op == TK_NOT_MATCH) {
    std::string lhs_buffer , rhs_buffer;
    if(!Transpile(*node.lhs,indent,&lhs_buffer) ||
       !Transpile(*node.rhs,indent,&rhs_buffer))
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
    if(!Transpile(*node.lhs,indent,&lhs_buffer) ||
       !Transpile(*node.rhs,indent,&rhs_buffer))
      return false;

    FormatAppend(output,"%s.add(%s,%s)",m_opt.runtime_namespace.c_str(),
                                        lhs_buffer.c_str(),
                                        rhs_buffer.c_str());
  } else {
    std::string arg_buffer;
    output->push_back('(');
    if(!Transpile(*node.lhs,indent,&arg_buffer)) return false;
    switch(node.op) {
      case TK_SUB: output->push_back('-'); break;
      case TK_MUL: output->push_back('*'); break;
      case TK_DIV: output->push_back('/'); break;
      case TK_MOD: output->push_back('%'); break;
      case TK_LT:  output->push_back('<'); break;
      case TK_LE:  output->append("<=")  ; break;
      case TK_GT:  output->push_back('>'); break;
      case TK_GE:  output->append(">=");   break;
      case TK_EQ:  output->appned("==");   break;
      case TK_NE:  output->append("!=");   break;
      default: VCL_UNREACHABLE(); break;
    }

    if(!Transpile(*node.rhs,indent,&arg_buffer)) return false;
    arg_buffer.push_back(')');
  }
  return true;
}

bool Transpiler::Transpile( const ast::Ternary& node , int indent , std::string* output ) {
  output->push_back('(');
  if(!Transpile(*node.condition,indent,output)) return false;
  output->append(" and ");
  if(!Transpile(*node.first,indent,output)) return false;
  output->append(" or ");
  if(!Transpile(*node.second,indent,output)) return false;
  output->push_back(')');
  return true;
}

bool Transpiler::Transpile( const ast::Prefix& node , size_t target , int indent ,
    std::string* output ) {
  std::string temp;

  target = target == 0 ? node.list.size() : target;

  for( size_t i = 0 ; i < target ; ++i ) {
    const ast::Prefix::Component& n = node.list.Index(i);
    switch(n.tag) {
      case ast::Prefix::Component::CALL:
        if(!Transpile(*n.funcall,indent,&output)) return false;
        break;
      case ast::Prefix::Component::INDEX:
        temp.push_back('[');
        if(!Transpile(*n.expression,indent,&output)) return false;
        temp.push_back(']');
        break;
      case ast::Prefix::Component::DOT:
        temp.push_back('.');
        temp.append(n.var->data());
        break;
      case ast::Prefix::Component::ATTRIBUTE:
        // Now we need to wrap the temporary buffer's stuff as a function call into a
        // {runtime}.get_attr(object,key) call since Lua doesn't support attribute syntax.
        FormatAppend(output,"%s.get_attr(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                 temp.c_str(),
                                                 n.var->data());
        temp.clear();
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
      return Transpile( static_cast<const ast::FuncCall&>(expr), indent, output );
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
    default:
      VCL_UNREACHABLE();
      return false;
  }
}

/* -------------------------------------------------------------------------------
 *
 * Statment
 *
 * ------------------------------------------------------------------------------*/

bool Transpiler::Transpile( const ast::Stmt& node , int indent ) {
  return Transpile(*node.expr,indent);
}

bool Transpiler::Transpile( const ast::Declare& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"declare",node.location);

  std::string buffer;
  if(!Transpile(*node.rhs,indent,&buffer)) return false;
  WriteLine( indent , "local %s = %s" , node.variable->data(), buffer.c_str() );
  return true;
}

bool Transpiler::Transpile( const ast::Set& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"set",node.location);

  std::string buffer;
  if(!Transpile(*node.rhs,indent,&buffer)) return false;

  if( node.lhs.type == ast::LeftHandSide::VARIABLE ) {
    WriteLine( indent , "%s = %s", node.lhs.variable->data(),
                                   buffer.c_str());
  } else {
    std::buffer object;

    const ast::Prefix::Component& last_comp = node.lhs.prefix->Last();

    // Check if we need to use set_attr to set the last attribute
    if( last_comp.tag == ast::Prefix::Component::ATTRIBUTE ) {

      // Transpile the component in the access chain up to the last component
      if(!Transpile(*node.lhs.prefix,node.lhs.prefix->list.size()-1,indent,&object))
        return false;

      // Handle the last component
      WriteLine( indent , "%s.set_attr( %s , %s , %s )", m_opt.runtime_namespace.c_str(),
                                                         object.c_str(),
                                                         last_comp.var->data(),
                                                         buffer.c_str() );
    } else {
      if(!Transpile(*node.lhs.prefix,0,indent,&object))
        return false;

      WriteLine( indent , "%s = %s", object.c_str() , buffer.c_str());
    }
  }
  return true;
}

bool Transpiler::Transpile( const ast::Unset& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"unset",node.location);

  // The unset is a little bit complicated because the way the unset work
  // is kind of anti-intuitive for Lua.
  if( node.lhs.type == ast::LeftHandSide::VARIABLE ) {

    // Due to the fact all primitive cannot be passed based on reference
    // so we have to generate unset code *inplace* for that variable to
    // make the unset actually work

    Template::Argument arg;

    arg["i1"] = Template::Str(GetIndent(indent));
    arg["i2"] = Template::Str(GetIndent(indent+1));
    arg["lb"] = Template::Str("\n");
    arg["obj"]= Template::Str(node.lhs.variable->data());

    CHECK( m_te.DoRender( "${i1}if type(${obj}) == \"string\" then${lb}"
                          "${i2}${obj} = nil${lb}"
                          "${il}elseif type(${obj}) == \"number\" then${lb}"
                          "${i2)${obj} = 0${lb}"
                          "${il}elseif tpye(${obj}) == \"boolean\" then${lb}"
                          "{$i2}${obj} = false${lb}"
                          "${i1}else${lb}"
                          "${i2}${obj} = nil${lb}"
                          "${i1}end${lb}" , arg, m_output) );
  } else {
    const ast::Prefix::Component& last_comp = node.lhs.prefix->Last();
    std::string buffer;

    if(!Transpile(*node.lhs.prefix,node.lhs.prefix->list.size()-1,indent,&buffer))
      return false;

    switch(last_comp.tag) {
      case ast::Prefix::Component::ATTRIBUTE:
        WriteLine(indent,"%s.unset_attr(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                buffer.c_str(),
                                                last_comp.var->data());
        break;
      case ast::Prefix::Component::INDEX:
        {
          std::string expr;
          if(!Transpile(*last_comp.expression,indent,&expr))
            return false;
          WriteLine(indent,"%s.unset_prop(%s,%s)",m_opt.runtime_namespace.c_str(),
                                                  buffer.c_str(),
                                                  expr.c_str());
        }
        break;
      case ast::Prefix::Component::DOT:
        WriteLine(indent,"%s.unset_prop(%s,%s)",m_opt.runtime_namespace.c_str(),
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

bool Transpiler::Transpile( const ast::Return& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"return",node.location);

  m_output->append( GetIndent(indent) );
  m_output->append( "return ");
  if(!ret.value) {
    m_output->append("nil");
    return true;
  } else {
    return TranspileExpression(*ret.value,indent,m_output);
  }
}

bool Transpiler::Transpile( const ast::Terminate& node, int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"terminate",node.location);

  switch(node.action) {
    case ACT_OK:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.ok_code);
      break;
    case ACT_FAIL:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.fail_code);
      break;
    case ACT_PIPE:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.pipe_code);
      break;
    case ACT_HASH:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.hash_code);
      break;
    case ACT_PURGE:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.purge_code);
      break;
    case ACT_LOOKUP:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.lookup_code);
      break;
    case ACT_RESTART:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.restart_code);
      break;
    case ACT_FETCH:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.fetch_code);
      break;
    case ACT_MISS:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.miss_code);
      break;
    case ACT_DELIVER:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.deliver_code);
      break;
    case ACT_RETRY:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.retry_code);
      break;
    case ACT_ABANDON:
      WriteLine(indent,"%s = %d",m_opt.vcl_terminate_code,m_opt.abandon_code);
      break;
    default:
      ReportError(node.location,"Unsupport terminated return!");
      break;
  }

  WriteLine(indent,"coroutine.yield()");
  return true;
}

bool Transpiler::Transpile( const ast::If& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"if",node.location);

  std::string condition;

  { // Leading if
    const ast::If::Branch& n = node.branch_list.First();
    if(!TranspileExpression(*n.condition,indent,&condition))
      return false;
    WriteLine(indent,"if %s then",condition.c_str());
    if(!TranspileChunk( *n.body , indent + 1 )) return false;
  }

  // Rest of elseif or else
  for( size_t i = 1 ; i < node.branch_list.size() ; ++i ) {
    const ast::If::Branch& n = node.branch_list.Index(i);
    if(n.condition) {
      condition.clear();
      if(!TranspileExpression(*n.condition,indent,&condition))
        return false;
      WriteLine(indent,"elseif %s then",condition.c_str());
      if(!TranspileChunk(*n.body,indent+1)) return false;
    } else {
      WriteLine(indent,"else");
      if(!TranspileChunk(*n.body,indent+1)) return false;
    }
  }

  WriteLine(indent,"end");
  return true;
}

bool Transpiler::Transpile( const ast::LexScope& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"scope",node.location);

  WriteLine(indent,"do");
  if(!TranspileChunk(*node.body,indent+1)) return false;
  WriteLine(indent,"end");
  return true;
}

bool Transpiler::Transpile( const ast::Import& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"import",node.location);

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

  std::string buffer;
  if(!TranspileExpression(*node.value,&buffer))
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
  arg["arg"] = Template::Value(NewExtensionInitializerGenerator(node,indent));
  arg["obj"] = Template::Value(node.instance_name->data());
  WriteTemplateLine(indent,"${obj} = ${ns}.extension.${name}(${arg})",arg);
  return true;
}

bool Transpiler::Transpile( const ast::Sub& node , int indent ) {
  return Transpile(*node.body,indent);
}

bool Transpiler::TranspileAnonymousSub( const ast::Sub& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"anonymous_sub",node.location);

  {
    Template::Argument arg;
    arg["arg"] = Template::Value( NewFunctionPrototype(node,indent) );
    WriteTemplateLine(indent,"function(${arg})",arg);
  }

  if(!TranspileChunk(*node.body,indent+1)) return false;
  WriteLinde(indent,"end");
  return true;
}


bool Transpiler::Transpile( const ast::AST& node , int indent ) {
  switch(node.type) {
    case ast::AST_TERMINATE:
      return Transpile( static_cast<const ast::Terminate&>(node) , indent );
    case ast::AST_RETURN:
      return Transpile( static_cast<const ast::Return&>(node), indent );
    case ast::AST_SET:
      return Transpile( static_cast<const ast::Set&>(node), indent);
    case ast::AST_UNSET:
      return Transpile( static_cast<const ast::Unset&>(node), indent);
    case ast::AST_DECLARE:
      return Transpile( static_cast<const ast::Declare&>(node), indent);
    case ast::AST_IF:
      return Transpile( static_cast<const ast::If&>(node) , indent);
    case ast::AST_STMT:
      return Transpile( static_cast<const ast::Stmt&>(node), indent);
    case ast::AST_IMPORT:
      return Transpile( static_cast<const ast::Import&>(node), indent);
    case ast::AST_EXTENSION:
      return Transpile( static_cast<const ast::Exntesion&>(node), indent);
    case ast::AST_GLOBAL:
      return Transpile( static_cast<const ast::Global&>(node),indent);
    default:
      ReportError(node.location,"Statement: %s doesn't support in transpilation!",
                                GetASTName( node.type ));
      return false;
  }
}

bool Transpiler::TranspileChunk( const ast::Chunk& node , int indent ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),"chunk",node.location);

  for( size_t i = 0 ; i < node.statement_list.size() ; ++i ) {
    if(!Transpile(*node.statement_list.Index(i),indent))
      return false;
  }

  return true;
}

bool Transpiler::Transpile( const ast::CompilationUnit::SubList& node ) {
  Comment comment(m_output,indent);
  comment.Line(CurrentSourceFile(),node.location,"sub(%s)",
                                                 node.front()->sub_name->data());

  // Generate sub's argument list
  {
    Template::Argument arg;
    arg["arg"] = Template::Value( NewFunctionPrototype(*node.front(),0) );
    arg["name"]= Template::Value( node.front()->sub_name()->data() );
    WriteTemplateLine(0,"function ${name}(${arg})",arg);
  }

  // Generate body of each 
}


















} // namespace

} // namespace lua51
} // namespace transpiler
} // namespace vm
} // namespace vcl
