#include <vm/lexer.h>
#include <gtest/gtest.h>

#define STRINGIFY(...) #__VA_ARGS__

namespace vcl {
namespace vm  {

using namespace vcl::util;

TEST(Lexer,Operators) {
  const std::string source = STRINGIFY(
      + - * / % ~ !~ == != < <= > >= =
      && || ! /= *= -= += %= ; , .
      () [] {} : ::
      );
  const std::string file_name = "test";
  Token tokens[] = {
    TK_ADD,
    TK_SUB,
    TK_MUL,
    TK_DIV,
    TK_MOD,
    TK_MATCH,
    TK_NOT_MATCH,
    TK_EQ,
    TK_NE,
    TK_LT,
    TK_LE,
    TK_GT,
    TK_GE,
    TK_ASSIGN,
    TK_AND,
    TK_OR,
    TK_NOT,
    TK_SELF_DIV,
    TK_SELF_MUL,
    TK_SELF_SUB,
    TK_SELF_ADD,
    TK_SELF_MOD,
    TK_SEMICOLON,
    TK_COMMA,
    TK_DOT,
    TK_LPAR,
    TK_RPAR,
    TK_LSQR,
    TK_RSQR,
    TK_LBRA,
    TK_RBRA,
    TK_COLON,
    TK_FIELD,
    TK_EOF
  };
  Lexer lexer(source,file_name);
  int i = 0;
  do {
    ASSERT_EQ(tokens[i],lexer.Next().token) <<
      GetTokenName(tokens[i])<<":"<<
      GetTokenName(lexer.lexeme().token)<<";";
    ++i;
  } while(tokens[i] != TK_EOF);
}

TEST(Lexer,Keyword) {
  const std::string source = STRINGIFY(
      sub call return new set unset
      vcl acl if declare elif elsif elseif
      import include global true false null
      for break continue
      );
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  Token tokens[] = {
    TK_SUB_ROUTINE,
    TK_CALL,
    TK_RETURN,
    TK_NEW,
    TK_SET,
    TK_UNSET,
    TK_VCL,
    TK_ACL,
    TK_IF,
    TK_DECLARE,
    TK_ELIF,
    TK_ELSIF,
    TK_ELSEIF,
    TK_IMPORT,
    TK_INCLUDE,
    TK_GLOBAL,
    TK_TRUE,
    TK_FALSE,
    TK_NULL,
    TK_FOR,
    TK_BREAK,
    TK_CONTINUE,
    TK_EOF
  };
  int i = 0;
  do {
    ASSERT_EQ(tokens[i],lexer.Next().token) <<
      '('<<i<<')'<<
      GetTokenName(tokens[i])<<":"<<
      GetTokenName(lexer.lexeme().token)<<";";
    ++i;
  } while(tokens[i] != TK_EOF);
}

TEST(Lexer,Variable) {
  const std::string source = STRINGIFY(
      sub sub_ sub2
      call call_
      return return_
      new new123
      set set2
      unset unset3
      vcl vcl10
      acl acl_
      if if_
      declare declare_
      elif elif_
      elsif elsif_
      elseif elseif_
      import import_
      include includex
      global globa2
      true tru3
      false fals_
      null nul_
      );
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_SUB_ROUTINE);
  ASSERT_EQ(lexer.Next().token,TK_VAR)<<GetTokenName(lexer.lexeme().token);
  ASSERT_EQ(lexer.lexeme().string(),"sub_");
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"sub2");

  ASSERT_EQ(lexer.Next().token,TK_CALL);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"call_");

  ASSERT_EQ(lexer.Next().token,TK_RETURN);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"return_");

  ASSERT_EQ(lexer.Next().token,TK_NEW);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"new123");

  ASSERT_EQ(lexer.Next().token,TK_SET);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"set2");

  ASSERT_EQ(lexer.Next().token,TK_UNSET);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"unset3");

  ASSERT_EQ(lexer.Next().token,TK_VCL);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"vcl10");

  ASSERT_EQ(lexer.Next().token,TK_ACL);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"acl_");

  ASSERT_EQ(lexer.Next().token,TK_IF);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"if_");

  ASSERT_EQ(lexer.Next().token,TK_DECLARE);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"declare_");

  ASSERT_EQ(lexer.Next().token,TK_ELIF);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"elif_");

  ASSERT_EQ(lexer.Next().token,TK_ELSIF);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"elsif_");

  ASSERT_EQ(lexer.Next().token,TK_ELSEIF);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"elseif_");

  ASSERT_EQ(lexer.Next().token,TK_IMPORT);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"import_");

  ASSERT_EQ(lexer.Next().token,TK_INCLUDE);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"includex");

  ASSERT_EQ(lexer.Next().token,TK_GLOBAL);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"globa2");

  ASSERT_EQ(lexer.Next().token,TK_TRUE);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"tru3");

  ASSERT_EQ(lexer.Next().token,TK_FALSE);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"fals_");

  ASSERT_EQ(lexer.Next().token,TK_NULL);
  ASSERT_EQ(lexer.Next().token,TK_VAR);
  ASSERT_EQ(lexer.lexeme().string(),"nul_");
  ASSERT_EQ(lexer.Next().token,TK_EOF);
}

// Literals
TEST(Lexer,SLString) {
  const std::string source = STRINGIFY(
      "" "abc" "a\"" "\"b" "a\"b"
      );
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_STRING);
  ASSERT_EQ(lexer.lexeme().string(),"");
  ASSERT_EQ(lexer.Next().token,TK_STRING);
  ASSERT_EQ(lexer.lexeme().string(),"abc");
  ASSERT_EQ(lexer.Next().token,TK_STRING);
  ASSERT_EQ(lexer.lexeme().string(),"a\"");
  ASSERT_EQ(lexer.Next().token,TK_STRING);
  ASSERT_EQ(lexer.lexeme().string(),"\"b");
  ASSERT_EQ(lexer.Next().token,TK_STRING);
  ASSERT_EQ(lexer.lexeme().string(),"a\"b");
  ASSERT_EQ(lexer.Next().token,TK_EOF);
}

TEST(Lexer,MLString) {
  const std::string source = "{\"\" }abcd \n efghd \n aaaa \n \"} \"single\"";
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_STRING) << GetTokenName(lexer.lexeme().token)<<lexer.lexeme().string();
  ASSERT_EQ(lexer.lexeme().string(),"\" }abcd \n efghd \n aaaa \n ");
  ASSERT_EQ(lexer.Next().token,TK_STRING);
  ASSERT_EQ(lexer.lexeme().string(),"single");
  ASSERT_EQ(lexer.Next().token,TK_EOF);
}

TEST(Lexer,Number) {
  const std::string source = "1234 1234.5 0 0.0";
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_INTEGER);
  ASSERT_EQ(lexer.lexeme().integer(),1234);
  ASSERT_EQ(lexer.Next().token,TK_REAL);
  ASSERT_EQ(lexer.lexeme().real(),1234.5);
  ASSERT_EQ(lexer.Next().token,TK_INTEGER);
  ASSERT_EQ(lexer.lexeme().integer(),0);
  ASSERT_EQ(lexer.Next().token,TK_REAL);
  ASSERT_EQ(lexer.lexeme().real(),0.0);
}

TEST(Lexer,Size) {
  const std::string source = "1gb2MB3kb4B 2GB 4MB3B 2B 23KB1B 2KB2KB3GB";
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(1,2,3,4));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(2));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(0,4,0,3));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(0,0,0,2));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(0,0,23,1));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(0,0,2,0));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(0,0,2,0));
  ASSERT_EQ(lexer.Next().token,TK_SIZE);
  ASSERT_EQ(lexer.lexeme().size(),Size(3,0,0,0));
}

TEST(Lexer,Duration) {
  const std::string source = "123 123s 23ms 1s3ms 1s2s 3ms1s 3ms2ms " \
                             "1h 2min 1h2min 2min1h " \
                             "1h2s 1h3ms 2min2s 10min2ms" \
                             "1h2min3s4ms 1h5min3s";
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_INTEGER);
  ASSERT_EQ(lexer.lexeme().integer(),123);
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,123,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,0,23));
  ASSERT_EQ(lexer.Next().token,TK_DURATION)<<lexer.lexeme().string();
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,1,3));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,1,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,2,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,0,3));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,1,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,0,3));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,0,0,2));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,0,0,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,2,0,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,2,0,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,2,0,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,0,0,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,0,2,0)) << lexer.lexeme().duration();
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,0,0,3));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,2,2,0));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(0,10,0,2));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,2,3,4));
  ASSERT_EQ(lexer.Next().token,TK_DURATION);
  ASSERT_EQ(lexer.lexeme().duration(),Duration(1,5,3,0));
  ASSERT_EQ(lexer.Next().token,TK_EOF);
}

TEST(Lexer,ExtendedVar) {
  const std::string source = "a-b-f-e.X-httpf.U-xf.U---";
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_TRUE(lexer.TryTokenAsExtendedVar());
  ASSERT_EQ(lexer.lexeme().string(),"a-b-f-e");
  ASSERT_EQ(lexer.Next().token,TK_DOT);
  ASSERT_TRUE(lexer.TryTokenAsExtendedVar());
  ASSERT_EQ(lexer.lexeme().string(),"X-httpf");
  ASSERT_EQ(lexer.Next().token,TK_DOT);
  ASSERT_TRUE(lexer.TryTokenAsExtendedVar());
  ASSERT_EQ(lexer.lexeme().string(),"U-xf");
  ASSERT_EQ(lexer.Next().token,TK_DOT);
  ASSERT_TRUE(lexer.TryTokenAsExtendedVar());
  ASSERT_EQ(lexer.lexeme().string(),"U---");
  ASSERT_EQ(lexer.Next().token,TK_EOF);
}

TEST(Lexer,Comments) {
  const std::string source = "# this is a line based \n"
                             "+ # this is another line based \n"
                             "- // Also a line based comments\n"
                             "+ /* This not line based \nAnother comments */\n"
                             "1 /* This is comments */ 2";
  const std::string file_name = "test";
  Lexer lexer(source,file_name);
  ASSERT_EQ(lexer.Next().token,TK_ADD);
  ASSERT_EQ(lexer.Next().token,TK_SUB);
  ASSERT_EQ(lexer.Next().token,TK_ADD);
  ASSERT_EQ(lexer.Next().token,TK_INTEGER);
  ASSERT_EQ(lexer.Next().token,TK_INTEGER);
  ASSERT_EQ(lexer.Next().token,TK_EOF);
}

TEST(Lexer,CommentsCornerCase) {
  {
    const std::string source = "# This is a line based";
    const std::string file_name = "test";
    Lexer lexer(source,file_name);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "// This is a line based";
    const std::string file_name = "test";
    Lexer lexer(source,file_name);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "/* This is a line based */";
    const std::string file_name = "test";
    Lexer lexer(source,file_name);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
}

TEST(Lexer,StringInterpolation) {
  {
    const std::string source = "''"; // Empty string interpolation
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END  );
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "'a'"; // One string segment
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_SEGMENT);
    ASSERT_EQ(lexer.lexeme().string(),"a");
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "'${a}'";
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_CODE_START);
    ASSERT_EQ(lexer.Next().token,TK_VAR);
    ASSERT_EQ(lexer.lexeme().string(),"a");
    ASSERT_EQ(lexer.Next().token,TK_RBRA);
    lexer.SetCodeEnd();
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "'ABCDE${a}'";
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_SEGMENT);
    ASSERT_EQ(lexer.lexeme().string(),"ABCDE");
    ASSERT_EQ(lexer.Next().token,TK_CODE_START);
    ASSERT_EQ(lexer.Next().token,TK_VAR);
    ASSERT_EQ(lexer.lexeme().string(),"a");
    ASSERT_EQ(lexer.Next().token,TK_RBRA);
    lexer.SetCodeEnd();
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "'ABCDE${a}ABCDE'";
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_SEGMENT);
    ASSERT_EQ(lexer.lexeme().string(),"ABCDE");
    ASSERT_EQ(lexer.Next().token,TK_CODE_START);
    ASSERT_EQ(lexer.Next().token,TK_VAR);
    ASSERT_EQ(lexer.lexeme().string(),"a");
    ASSERT_EQ(lexer.Next().token,TK_RBRA);
    lexer.SetCodeEnd();
    ASSERT_EQ(lexer.Next().token,TK_SEGMENT);
    ASSERT_EQ(lexer.lexeme().string(),"ABCDE");
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  {
    const std::string source = "'${a}${b}'";
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_CODE_START);
    ASSERT_EQ(lexer.Next().token,TK_VAR);
    ASSERT_EQ(lexer.lexeme().string(),"a");
    ASSERT_EQ(lexer.Next().token,TK_RBRA);
    lexer.SetCodeEnd();
    ASSERT_EQ(lexer.Next().token,TK_CODE_START);
    ASSERT_EQ(lexer.Next().token,TK_VAR);
    ASSERT_EQ(lexer.lexeme().string(),"b");
    ASSERT_EQ(lexer.Next().token,TK_RBRA);
    lexer.SetCodeEnd();
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
  // Escape characters inside of string interpolation
  {
    const std::string source = "'\\'\\\\${a}\\'\\\\'";
    const std::string test = "test";
    Lexer lexer(source,test);
    ASSERT_EQ(lexer.Next().token,TK_INTERP_START);
    ASSERT_EQ(lexer.Next().token,TK_SEGMENT);
    ASSERT_EQ(lexer.lexeme().string(),"'\\");
    ASSERT_EQ(lexer.Next().token,TK_CODE_START);
    ASSERT_EQ(lexer.Next().token,TK_VAR);
    ASSERT_EQ(lexer.lexeme().string(),"a");
    ASSERT_EQ(lexer.Next().token,TK_RBRA);
    lexer.SetCodeEnd();
    ASSERT_EQ(lexer.Next().token,TK_SEGMENT);
    ASSERT_EQ(lexer.lexeme().string(),"'\\");
    ASSERT_EQ(lexer.Next().token,TK_INTERP_END);
    ASSERT_EQ(lexer.Next().token,TK_EOF);
  }
}

} // namespace vm
} // namespace vcl

int main( int argc, char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
