#include <gtest/gtest.h>
#include <vcl/vcl.h>
#include <string>
#include <cstdlib>
#include <unordered_map>
#include <time.h>

namespace vcl {

std::string RandStr() {
  std::string buf;
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < 32; ++i) {
		buf.push_back(alphanum[rand() % (sizeof(alphanum) - 1)]);
	}
	return buf;
}

struct HorribleStringHash {
  uint32_t operator () ( const char* string , size_t length ) const {
    VCL_UNUSED(string);
    VCL_UNUSED(length);
    return 1;
  }
};

uint64_t Now() {
  timespec sp;
  clock_gettime(CLOCK_MONOTONIC,&sp);
  return static_cast<uint64_t>(sp.tv_sec*1000000 + sp.tv_nsec/1000);
}

TEST(VCL,StringDict) {
  ContextGC gc(100000000,1.0,NULL);

  {
    StringDict<int> string_dict;

    ASSERT_EQ(string_dict.size(),0);
    ASSERT_EQ(string_dict.capacity(),4);

    ASSERT_TRUE( string_dict.Insert(&gc,"a",1) );
    ASSERT_EQ( string_dict.size() , 1);

    ASSERT_TRUE( string_dict.Find("a") != NULL );
    ASSERT_TRUE( *string_dict.Find("a") == 1 );

    ASSERT_TRUE( string_dict.Insert(&gc,"b",2) );
    ASSERT_EQ( string_dict.size() , 2);

    ASSERT_TRUE( string_dict.Find("b") != NULL );
    ASSERT_TRUE( *string_dict.Find("b") == 2 );

    ASSERT_TRUE( string_dict.Insert(&gc,"c",3) );
    ASSERT_EQ( string_dict.size() , 3);

    ASSERT_TRUE( string_dict.Find("c") != NULL );
    ASSERT_TRUE( *string_dict.Find("c") == 3 );

    ASSERT_TRUE( string_dict.Insert(&gc,"d",4) );
    ASSERT_EQ( string_dict.size() , 4);

    ASSERT_TRUE( string_dict.Find("d") != NULL );
    ASSERT_TRUE( *string_dict.Find("d") == 4 );

    string_dict.Clear();

    ASSERT_TRUE( string_dict.Insert(&gc,"aaaa",1) );
    ASSERT_TRUE( string_dict.Insert(&gc,"bbbb",2) );
    ASSERT_TRUE( string_dict.Insert(&gc,"cccc",3) );
    ASSERT_TRUE( string_dict.Insert(&gc,"xxxx",4) );

    ASSERT_TRUE( string_dict.Find("aaaa") );
    ASSERT_TRUE( string_dict.Find("bbbb") );
    ASSERT_TRUE( string_dict.Find("cccc") );
    ASSERT_TRUE( string_dict.Find("xxxx") );
  }

  {
    StringDict<int> string_dict(2);
    ASSERT_EQ( string_dict.size() , 0 );
    ASSERT_EQ( string_dict.capacity() , 2 );

    ASSERT_TRUE( string_dict.Insert(&gc,"aaaa",1) );
    ASSERT_TRUE( string_dict.Insert(&gc,"bbbb",2) );

    ASSERT_EQ( string_dict.size() , 2 );
    ASSERT_EQ( string_dict.capacity() , 2 );

    ASSERT_TRUE( string_dict.Find("aaaa") );
    ASSERT_TRUE( string_dict.Find("bbbb") );

    ASSERT_TRUE( string_dict.Insert(&gc,"xxxx",3) );
    ASSERT_TRUE( string_dict.Insert(&gc,"zzzz",4) );
    ASSERT_EQ( string_dict.size() , 4 );
    ASSERT_EQ( string_dict.capacity(),4);

    ASSERT_TRUE( string_dict.Find("aaaa") );
    ASSERT_TRUE( string_dict.Find("bbbb") );
    ASSERT_TRUE( string_dict.Find("xxxx") );
    ASSERT_TRUE( string_dict.Find("zzzz") );
  }

  {
    StringDict<size_t> string_dict(2);
    std::map<std::string,size_t> name_set;

    for( size_t i = 0 ; i < 1024; ++i ) {
      srand(i*i+1000+i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if(itr == name_set.end()) {
        name_set.insert(std::make_pair(name,i+1));
        ASSERT_TRUE( string_dict.Insert(&gc,name,i+1) );
      } else {
        string_dict.InsertOrUpdate(&gc,name,i+1);
        itr->second = i+1;
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *string_dict.Find(itr->first) == itr->second );
    }
  }

  { // All collided together forms a chain , this is extream situations
    StringDict<size_t,HorribleStringHash> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100+i*i*i+777);
      std::string name(RandStr());
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if(itr == name_set.end()) {
        name_set.insert( std::make_pair(name,i+1) );
        ASSERT_TRUE( horrible_dict.Insert(&gc,name,i+1) );
      } else {
        horrible_dict.InsertOrUpdate(&gc,name,i+1);
        itr->second = i+1;
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
    }
  }

  // Update test
  {
    StringDict<size_t> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        ASSERT_TRUE( horrible_dict.Insert(&gc,name,i+1) );
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
      ASSERT_TRUE( horrible_dict.Update(&gc,itr->first,77777) );
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == 77777 );
    }
  }
  {
    StringDict<size_t,HorribleStringHash> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        ASSERT_TRUE( horrible_dict.Insert(&gc,name,i+1) );
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
      ASSERT_TRUE( horrible_dict.Update(&gc,itr->first,77777) );
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == 77777 );
    }
  }

  // InsertOrUpdate test
  {
    StringDict<size_t> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        horrible_dict.InsertOrUpdate( &gc , name , i+1 );
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
      horrible_dict.InsertOrUpdate(&gc,itr->first,77777);
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == 77777 );
    }
  }
  {
    StringDict<size_t,HorribleStringHash> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        horrible_dict.InsertOrUpdate( &gc , name , i+1 );
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
      horrible_dict.InsertOrUpdate(&gc,itr->first,77777);
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == 77777 );
    }
  }

  // Remove
  {
    StringDict<size_t> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        horrible_dict.InsertOrUpdate( &gc , name , i+1 );
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
      ASSERT_TRUE( horrible_dict.Remove(itr->first,NULL) );
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( horrible_dict.Find(itr->first) == NULL );
    }
  }

  {
    StringDict<size_t,HorribleStringHash> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        horrible_dict.InsertOrUpdate( &gc , name , i+1 );
      }
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( *horrible_dict.Find(itr->first) == itr->second );
      ASSERT_TRUE( horrible_dict.Remove(itr->first,NULL) );
    }

    for( std::map<std::string,size_t>::iterator itr = name_set.begin();
        itr != name_set.end() ; ++itr ) {
      ASSERT_TRUE( horrible_dict.Find(itr->first) == NULL );
    }
  }

  // Iterator
  {
    StringDict<size_t,HorribleStringHash> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        horrible_dict.InsertOrUpdate( &gc , name , i+1 );
      }
    }

    size_t count = 0;

    for( StringDict<size_t,HorribleStringHash>::Iterator itr = horrible_dict.Begin();
        itr != horrible_dict.End() ; ++itr ) {
      ASSERT_NE( name_set.end() , name_set.find( itr->first->ToStdString() ) );
      ASSERT_EQ( name_set.find( itr->first->ToStdString() )->second ,
                 itr->second );
      ++count;
    }

    ASSERT_EQ(count,horrible_dict.size());
  }

  {
    StringDict<size_t> horrible_dict;
    std::map<std::string,size_t> name_set;
    for( size_t i = 0 ; i < 1000 ; ++i ) {
      srand(100 + i * i);
      std::string name( RandStr() );
      std::map<std::string,size_t>::iterator itr = name_set.find(name);
      if( itr == name_set.end() ) {
        name_set.insert( std::make_pair(name,i+1) );
        horrible_dict.InsertOrUpdate( &gc , name , i+1 );
      }
    }

    size_t count = 0;

    const StringDict<size_t>& cdict = horrible_dict;

    for( StringDict<size_t>::ConstIterator itr = cdict.Begin();
        itr != cdict.End() ; ++itr ) {
      ASSERT_NE( name_set.end() , name_set.find( itr->first->ToStdString() ) );
      ASSERT_EQ( name_set.find( itr->first->ToStdString() )->second ,
                 itr->second );
      ++count;
    }

    ASSERT_EQ(count,horrible_dict.size());
  }
}

TEST(VCL,Value) {
  ContextGC gc(10000,1.0,NULL);
  {
    Value v;
    ASSERT_TRUE(v.IsNull());
  }
  {
    Value v(1000);
    ASSERT_TRUE(v.IsInteger());
    ASSERT_EQ(1000,v.GetInteger());
  }
  {
    Value v(100.0);
    ASSERT_TRUE(v.IsReal());
    ASSERT_EQ(100.0,v.GetReal());
  }
  {
    Value v(true);
    ASSERT_TRUE(v.IsBoolean());
    ASSERT_EQ(true,v.GetBoolean());
  }
  {
    Value v(false);
    ASSERT_TRUE(v.IsBoolean());
    ASSERT_EQ(false,v.GetBoolean());
  }
  {
    Value v(vcl::util::Size(1,1,1,1));
    ASSERT_TRUE(v.IsSize());
    ASSERT_TRUE(v.GetSize().gigabytes == 1);
    ASSERT_TRUE(v.GetSize().megabytes == 1);
    ASSERT_TRUE(v.GetSize().kilobytes == 1);
    ASSERT_TRUE(v.GetSize().bytes ==  1);
  }
  {
    Value v(vcl::util::Duration(0,0,1,1));
    ASSERT_TRUE(v.IsDuration());
    ASSERT_TRUE(v.GetDuration().second == 1);
    ASSERT_TRUE(v.GetDuration().millisecond == 1);
  }
  {
    Value v( gc.NewString("hello world") );
    ASSERT_TRUE(v.IsString());
    ASSERT_TRUE(v.GetString()->ToStdString() == "hello world");
  }
  {
    Value v( gc.NewAction( ACT_PIPE ) );
    ASSERT_TRUE(v.IsAction());
    ASSERT_TRUE(v.GetAction()->action_code() == ACT_PIPE);
    Value v2( gc.NewAction( ACT_MISS ) );
    ASSERT_TRUE(v2.IsAction());
    ASSERT_TRUE(v2.GetAction()->action_code() == ACT_MISS);
  }
  {
    Value v( gc.NewList(1) );
    ASSERT_TRUE(v.IsList());
    ASSERT_TRUE(v.GetList()->size() == 0);
    ASSERT_TRUE(v.GetList()->empty());
  }
  {
    Value v( gc.NewDict() );
    ASSERT_TRUE(v.IsDict());
    ASSERT_TRUE(v.GetDict()->size() == 0);
    ASSERT_TRUE(v.GetDict()->empty());
  }
  {
    Value v( gc.NewModule("std") );
    ASSERT_TRUE(v.IsModule());
    ASSERT_TRUE(v.GetModule());
  }
  // Modification
  {
    Value v( 10 );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ( v.GetInteger(), 10);

    v.SetReal(20.0);
    ASSERT_TRUE( v.IsReal() );
    ASSERT_EQ( v.GetReal() , 20.0 );

    v.SetBoolean(true);
    ASSERT_TRUE( v.IsBoolean() );
    ASSERT_EQ( v.GetBoolean() , true );

    v.SetNull();
    ASSERT_TRUE( v.IsNull() );

    v.SetString( gc.NewString("Hello") );
    ASSERT_TRUE( v.IsString() );
    ASSERT_EQ( *v.GetString() , "Hello" );

    v.SetList( gc.NewList(10) );
    ASSERT_TRUE( v.IsList() );
    ASSERT_TRUE( v.GetList()->empty() );

    v.SetDict( gc.NewDict() );
    ASSERT_TRUE( v.IsDict() );
    ASSERT_TRUE( v.GetDict()->empty() );
  }
}

TEST(VCL,ValueAPI_Primitive) {
  ContextGC gc(10000,1.0,NULL);

  Dict* dict = gc.NewDict();
  ASSERT_TRUE( dict->Insert( *gc.NewString("A"),Value(1)) );
  ASSERT_TRUE( dict->Insert( *gc.NewString("Z-X"),Value(true)) );
  ASSERT_TRUE( dict->Insert( *gc.NewString("XX-F"),Value()) );

  List* list = gc.NewList(4);
  list->Push( Value(1) );
  list->Push( Value(2) );
  list->Push( Value(false) );
  list->Push( Value( gc.NewString("MyString")) );

  {
    Value value;
    ASSERT_TRUE( Value(dict).GetProperty( NULL , *gc.NewString("A") , &value ) );
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ( 1 , value.GetInteger() );

    ASSERT_TRUE( Value(dict).SetProperty( NULL , *gc.NewString("XX") , Value(100)) );
    ASSERT_TRUE( Value(dict).GetProperty( NULL , *gc.NewString("XX") , &value ) );
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ( 100 , value.GetInteger() );
    ASSERT_TRUE( dict->Find("XX",&value) );
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ(100,value.GetInteger());

    ASSERT_TRUE( Value(dict).GetProperty( NULL , *gc.NewString("XX") , &value ) );
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ( 100 , value.GetInteger() );

    ASSERT_TRUE( dict->Find("XX",&value) );
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ(100,value.GetInteger());
  }

  {
    Value value;
    ASSERT_TRUE( Value(dict).GetAttribute( NULL , *gc.NewString("Z-X"), &value) );
    ASSERT_TRUE( value.IsBoolean() ) ;
    ASSERT_TRUE( value.GetBoolean() == true );

    ASSERT_TRUE( Value(dict).SetAttribute( NULL , *gc.NewString("X-Z"), Value()) );
    ASSERT_TRUE( Value(dict).GetAttribute( NULL , *gc.NewString("X-Z"), &value ) );
    ASSERT_TRUE( value.IsNull() );

    ASSERT_TRUE( dict->Find("X-Z",&value));
    ASSERT_TRUE( value.IsNull() );
  }

  {
    Value value;
    ASSERT_TRUE( Value(list).GetIndex(NULL,Value(0),&value));
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ( 1 , value.GetInteger()) ;

    ASSERT_TRUE( Value(list).SetIndex(NULL,Value(1),Value(1000)));
    ASSERT_TRUE( Value(list).GetIndex(NULL,Value(1),&value) );
    ASSERT_TRUE( value.IsInteger() );
    ASSERT_EQ( 1000 , value.GetInteger() );
    ASSERT_TRUE( list->Index(1).IsInteger() );
    ASSERT_TRUE( list->Index(1).GetInteger() == 1000 );
  }

  // Negative pattern
  {
    Value value;
#define XX(...) \
    do { \
      ASSERT_TRUE( Value(__VA_ARGS__).GetProperty(NULL,*gc.NewString("_"),&value).is_unimplemented() ); \
      ASSERT_TRUE( Value(__VA_ARGS__).SetProperty(NULL,*gc.NewString("_"),Value(1)).is_unimplemented() ); \
      ASSERT_TRUE( Value(__VA_ARGS__).GetIndex(NULL,Value(1),&value).is_unimplemented()); \
      ASSERT_TRUE( Value(__VA_ARGS__).SetIndex(NULL,Value(1),Value(1)).is_unimplemented()); \
      ASSERT_TRUE( Value(__VA_ARGS__).GetAttribute(NULL,*gc.NewString("_"),&value).is_unimplemented() ); \
      ASSERT_TRUE( Value(__VA_ARGS__).SetAttribute(NULL,*gc.NewString("_"),Value(1)).is_unimplemented()); \
    } while(false)

    XX(100);
    XX(true);
    XX(false);
    XX(Value());
    XX(100.0);
    XX(gc.NewString("string"));

#undef XX // XX
  }

  // Arithmatic
#define XX(OP,LHS,RHS,COP,TYPE) \
  do { \
    Value value; \
    ASSERT_TRUE( Value(LHS).OP( NULL , Value(RHS) , &value ) ); \
    ASSERT_TRUE( value.Is##TYPE() ); \
    ASSERT_EQ( (LHS) COP (RHS) , value.Get##TYPE() ); \
  } while(false)

  XX(Add,1,2,+,Integer);
  XX(Add,1.0,2.0,+,Real);
  XX(Add,1,2.0,+,Real);
  XX(Add,2.0,1,+,Real);
  XX(Add,true,false,+,Integer);
  XX(Add,false,true,+,Integer);
  XX(Add,false,10,+,Integer);
  XX(Add,100,true,+,Integer);
  XX(Add,false,1.0,+,Real);
  XX(Add,1.0,true,+,Real);

  XX(Sub,1,2,-,Integer);
  XX(Sub,1.0,2.0,-,Real);
  XX(Sub,1,2.0,-,Real);
  XX(Sub,2.0,1,-,Real);
  XX(Sub,true,false,-,Integer);
  XX(Sub,false,true,-,Integer);
  XX(Sub,true,10,-,Integer);
  XX(Sub,10,false,-,Integer);
  XX(Sub,true,1.0,-,Real);
  XX(Sub,1.0,false,-,Real);

  XX(Mul,1,2,*,Integer);
  XX(Mul,-1.0,-2.0,*,Real);
  XX(Mul,2,-1.0,*,Real);
  XX(Mul,-1.0,2,*,Real);
  XX(Mul,true,1,*,Integer);
  XX(Mul,1,false,*,Integer);
  XX(Mul,true,false,*,Integer);
  XX(Mul,true,-1.0,*,Real);
  XX(Mul,-0.0,false,*,Real);

  XX(Div,4,2,/,Integer);
  XX(Div,4.0,1.0,/,Real);
  XX(Div,4,-1.0,/,Real);
  XX(Div,-4.0,-1,/,Real);
  XX(Div,true,1,/,Integer);
  XX(Div,false,1,/,Integer);
  XX(Div,false,true,/,Integer);
  XX(Div,false,10.0,/,Real);
  XX(Div,10.0,true,/,Real);

  XX(Mod,4,2,%,Integer);
  XX(Mod,false,true,%,Integer);
  XX(Mod,true,100,%,Integer);
  XX(Mod,false,100,%,Integer);
  XX(Mod,100,true,%,Integer);

#undef XX // XX
  // Divide 0
#define XX(LHS) \
  do { \
    Value value; \
    ASSERT_TRUE( Value(LHS).Div( NULL , Value(0) , &value ).is_fail() ) ; \
    ASSERT_TRUE( Value(LHS).Div( NULL , Value(0.0),&value ).is_fail() ) ; \
    ASSERT_TRUE( Value(LHS).Div( NULL , Value(false),&value).is_fail() ); \
  } while(false)

  XX(1);
  XX(1.0);
  XX(true);
  XX(false);
#undef XX // XX

#define XX(LHS) \
  do { \
    Value value; \
    ASSERT_TRUE( Value(LHS).Mod( NULL , Value(0) , &value ).is_fail() ); \
    ASSERT_TRUE( Value(LHS).Mod( NULL , Value(false),&value).is_fail() ); \
  } while(false)
  XX(1);
  XX(true);
  XX(false);
#undef XX // XX

  {
    Value value;
    ASSERT_TRUE( Value(1.0).Mod( NULL , Value(0) , &value).is_fail() );
    ASSERT_TRUE( Value(1).Mod(NULL,Value(0.0),&value).is_fail() );
    ASSERT_TRUE( Value(1.0).Mod(NULL,Value(0.1),&value).is_fail() );
    ASSERT_TRUE( Value(true).Mod(NULL,Value(1.0),&value).is_fail() );
    ASSERT_TRUE( Value(1.0).Mod(NULL,Value(false),&value).is_fail() );
  }

  // ===============================================================
  // Aggregator operators
  // ===============================================================
#define XX(OP,LHS,RHS,COP,TYPE) \
  do { \
    Value lhs(LHS); \
    ASSERT_TRUE( lhs.OP( NULL , Value(RHS) ) ); \
    ASSERT_EQ( lhs.Get##TYPE() , (LHS) COP (RHS) ); \
  } while(false)

  XX(SelfAdd,1,2,+,Integer);
  XX(SelfAdd,1.0,2.0,+,Real);
  XX(SelfAdd,1.0,2,+,Real);
  XX(SelfAdd,2,1.0,+,Real);
  XX(SelfAdd,true,false,+,Integer);
  XX(SelfAdd,true,1,+,Integer);
  XX(SelfAdd,1,false,+,Integer);
  XX(SelfAdd,1.0,true,+,Real);
  XX(SelfAdd,false,1.0,+,Real);

  XX(SelfSub,1,2,-,Integer);
  XX(SelfSub,1.0,2.0,-,Real);
  XX(SelfSub,1.0,2,-,Real);
  XX(SelfSub,2,1.0,-,Real);
  XX(SelfSub,true,false,-,Integer);
  XX(SelfSub,true,1,-,Integer);
  XX(SelfSub,1,false,-,Integer);
  XX(SelfSub,1.0,true,-,Real);
  XX(SelfSub,false,1.0,-,Real);

  XX(SelfMul,1,2,*,Integer);
  XX(SelfMul,1.0,2.0,*,Real);
  XX(SelfMul,1.0,2,*,Real);
  XX(SelfMul,2,1.0,*,Real);
  XX(SelfMul,true,false,*,Integer);
  XX(SelfMul,true,1,*,Integer);
  XX(SelfMul,1,false,*,Integer);
  XX(SelfMul,1.0,true,*,Real);
  XX(SelfMul,false,1.0,*,Real);

  XX(SelfDiv,2,1,/,Integer);
  XX(SelfDiv,2.0,1.0,/,Real);
  XX(SelfDiv,2.0,1,/,Real);
  XX(SelfDiv,4,2.0,/,Real);
  XX(SelfDiv,false,true,/,Integer);
  XX(SelfDiv,true,1,/,Integer);
  XX(SelfDiv,1,true,/,Integer);
  XX(SelfDiv,true,1.0,/,Real);
  XX(SelfDiv,1.0,true,/,Real);

  XX(SelfMod,2,1,%,Integer);
  XX(SelfMod,false,true,%,Integer);
  XX(SelfMod,true,1,%,Integer);
  XX(SelfMod,100,true,%,Integer);

#undef XX // XX

  // Divide 0
#define XX(LHS) \
  do { \
    Value v(LHS); \
    ASSERT_TRUE( v.SelfDiv( NULL , Value(0) ).is_fail() ); \
    ASSERT_TRUE( v.SelfDiv( NULL , Value(0.0)).is_fail() ); \
    ASSERT_TRUE( v.SelfDiv( NULL , Value(false)).is_fail() ); \
  } while(false)

  XX(1);
  XX(1.0);
  XX(true);
  XX(false);
  XX(0.0);
  XX(0);

#undef XX // XX

#define XX(LHS) \
  do { \
    Value v(LHS); \
    ASSERT_TRUE ( v.SelfMod( NULL , Value(0) ).is_fail() ) ; \
    ASSERT_TRUE ( v.SelfMod( NULL , Value(false) ).is_fail() ) ; \
  } while(false)

  XX(1);
  XX(0);
  XX(true);
  XX(false);

#undef XX // XX

  { // Mod with real operators
    ASSERT_FALSE( Value(1).SelfMod( NULL , Value(1.0) ) );
    ASSERT_FALSE( Value(1.0).SelfMod(NULL, Value(1)));
    ASSERT_FALSE( Value(true).SelfMod(NULL,Value(1.0)));
    ASSERT_FALSE( Value(1.0).SelfMod(NULL,Value(true)));
    ASSERT_FALSE( Value(1.0).SelfMod(NULL,Value(1.0)));
  }

  // Match and NotMatch should be always negative for primitive type
#define XX(...) \
  do { \
    Value v(  (__VA_ARGS__)  ); \
    bool result; \
    ASSERT_FALSE( v.Match( NULL , Value(1) , &result ) ); \
    ASSERT_FALSE( v.NotMatch( NULL , Value(2) , &result ) ); \
  } while(false)

  XX(1);
  XX(1.0);
  XX(true);
  XX(false);
  XX( Value() );
  XX( gc.NewList(10) );
  XX( gc.NewDict() );
  XX( gc.NewModule("xx") );

#undef XX // XX

  {
    Value v(1);
    ASSERT_TRUE( v.Unset(NULL) );
    ASSERT_EQ(0,v.GetInteger());
  }
  {
    Value v(1.0);
    ASSERT_TRUE( v.Unset(NULL) );
    ASSERT_EQ(0.0,v.GetReal());
  }
  {
    Value v(true);
    ASSERT_TRUE( v.Unset(NULL) );
    ASSERT_TRUE( v.GetBoolean() == false );
  }
  {
    Value v(false);
    ASSERT_TRUE( v.Unset(NULL) );
    ASSERT_TRUE( v.GetBoolean() == false );
  }
  {
    Value v;
    ASSERT_TRUE( v.Unset(NULL) );
    ASSERT_TRUE( v.IsNull() );
  }

#define XX(OP,COP) \
  do { \
    bool result; \
    ASSERT_TRUE( Value(1.1).OP( NULL , Value(1.0) , &result ) ); \
    ASSERT_TRUE( result == ( 1.1 COP 1.0 ) );  \
    ASSERT_TRUE( Value(1).OP( NULL , Value(0) , &result ) ); \
    ASSERT_TRUE( result == ( 1.0 COP 0.0 ) ); \
    ASSERT_TRUE( Value(1.0).OP( NULL , Value(0) , &result ) ); \
    ASSERT_TRUE( result == ( 1.0 COP 0 ) ); \
    ASSERT_TRUE( Value(1).OP( NULL , Value(1.1) , &result )) ; \
    ASSERT_TRUE( result == ( 1 COP 1.1 ) ); \
    ASSERT_TRUE( Value(1).OP( NULL ,Value(true) , &result )); \
    ASSERT_TRUE( result == ( 1 COP true ) ); \
    ASSERT_TRUE( Value(true).OP( NULL , Value(1) , &result)); \
    ASSERT_TRUE( result == ( true COP 1 )); \
    ASSERT_TRUE( Value(1.1).OP( NULL , Value(true), &result)); \
    ASSERT_TRUE( result == ( 1.1 COP static_cast<double>(true)) ); \
    ASSERT_TRUE( Value(true).OP( NULL , Value(1.1), &result)); \
    ASSERT_TRUE( result == ( static_cast<double>(true) COP 1.1 ) ); \
  } while(false)

  XX(Less,<);
  XX(LessEqual,<=);
  XX(Greater,>);
  XX(GreaterEqual,>=);
  XX(Equal,==);
  XX(NotEqual,!=);

#undef XX // XX

  // ======================================================
  // Conversion
  // ======================================================

  {
    std::string value;
    ASSERT_FALSE( Value(1).ToString(NULL,&value) );
    ASSERT_FALSE( Value(1.0).ToString(NULL,&value) );
    ASSERT_FALSE( Value(true).ToString(NULL,&value) );
    ASSERT_FALSE( Value(false).ToString(NULL,&value) );
    ASSERT_FALSE( Value().ToString(NULL,&value) );
  }

  {
    bool value;
    ASSERT_TRUE( Value(1).ToBoolean(NULL,&value) );
    ASSERT_TRUE( value );

    ASSERT_TRUE( Value(0).ToBoolean(NULL,&value) );
    ASSERT_FALSE( value );

    ASSERT_TRUE( Value(true).ToBoolean(NULL,&value) );
    ASSERT_TRUE( value );

    ASSERT_TRUE( Value(false).ToBoolean(NULL,&value) );
    ASSERT_FALSE( value );

    ASSERT_TRUE( Value().ToBoolean(NULL,&value) );
    ASSERT_FALSE( value );

    ASSERT_TRUE( Value(1.0).ToBoolean(NULL,&value) );
    ASSERT_TRUE( value );

    ASSERT_TRUE( Value(0.0).ToBoolean(NULL,&value) );
    ASSERT_FALSE( value );

    ASSERT_TRUE( Value(0.1).ToBoolean(NULL,&value) );
    ASSERT_TRUE( value );
  }

  {
    int32_t i;
    double r;
    ASSERT_TRUE( Value(1).ToInteger(NULL,&i) );
    ASSERT_EQ(1,i);

    ASSERT_TRUE( Value(1).ToReal(NULL,&r) );
    ASSERT_EQ( 1.0, r );

    ASSERT_TRUE( Value(1.0).ToInteger(NULL,&i) );
    ASSERT_EQ( 1 , i );

    ASSERT_TRUE( Value(0.1).ToReal(NULL,&r) );
    ASSERT_EQ( 0.1 ,r );

    ASSERT_TRUE( Value(true).ToInteger(NULL,&i) );
    ASSERT_EQ( 1 , i );

    ASSERT_TRUE( Value(false).ToInteger(NULL,&i) );
    ASSERT_EQ( 0 , i );

    ASSERT_TRUE( Value(true).ToReal(NULL,&r) );
    ASSERT_EQ( 1.0 , r );

    ASSERT_TRUE( Value(false).ToReal(NULL,&r) );
    ASSERT_EQ( 0.0 , r );

    ASSERT_FALSE( Value().ToInteger(NULL,&i) );
    ASSERT_FALSE( Value().ToReal(NULL,&r) );
  }
}

TEST(VCL,String) {
  Context context(ContextOption() , boost::shared_ptr<CompiledCode>(
        new CompiledCode(NULL)));
  ContextGC* gc = context.gc();
  {
    ASSERT_EQ( strlen("Hello") , gc->NewString("Hello")->size() );
    ASSERT_FALSE( gc->NewString("Hello")->empty() );
    ASSERT_TRUE( gc->NewString("Hello")->ToStdString() == std::string("Hello") );
  }

  // Add operator
  {
    Value v;
    ASSERT_TRUE( gc->NewString("Hello")->Add( &context ,
          Value( gc->NewString("World") ) ,&v) );
    ASSERT_TRUE( v.IsString() );
    ASSERT_TRUE( v.GetString()->ToStdString() == "HelloWorld" );
    ASSERT_TRUE( v.GetString()->size() == strlen("HelloWorld") );
    ASSERT_FALSE( v.GetString()->empty() );

    String* string = gc->NewString("Hello");
    ASSERT_TRUE(string->SelfAdd(&context,Value( gc->NewString("World"))));
    ASSERT_TRUE( string->size() == strlen("HelloWorld") );
    ASSERT_FALSE(string->empty());
    ASSERT_EQ(*string,"HelloWorld");
  }

  // Match Operator
  {
    Value v( gc->NewString("Hello") );
    bool result;
    ASSERT_TRUE( v.Match( &context , Value( gc->NewString("Hello") ) , &result ) );
    ASSERT_TRUE( result );
    ASSERT_TRUE( v.Match( &context , Value( gc->NewString("HelloW") ), &result ) );
    ASSERT_FALSE( result );

    ASSERT_TRUE(v.NotMatch( &context , Value( gc->NewString("Hello") ) , &result));
    ASSERT_FALSE( result );

    ASSERT_TRUE(v.NotMatch( &context , Value( gc->NewString("HelloW") ) ,&result));
    ASSERT_TRUE( result );
  }

  {
    Value v( gc->NewString("[a-zA-Z]{2,3}") );
    bool result;

    ASSERT_TRUE( gc->NewString("ABC")->Match( &context , v , &result ) );
    ASSERT_TRUE( result );

    ASSERT_TRUE( gc->NewString("A")->Match( &context , v , &result ) );
    ASSERT_FALSE(result);

    ASSERT_TRUE( gc->NewString("12")->Match( &context , v , &result ) );
    ASSERT_FALSE(result);

    ASSERT_TRUE( gc->NewString("___")->NotMatch( &context , v , &result ));
    ASSERT_TRUE(result);

    ASSERT_TRUE( gc->NewString("CC")->NotMatch( &context , v , &result ));
    ASSERT_FALSE(result);

    ASSERT_TRUE( gc->NewString("XXX")->NotMatch( &context, v , &result ));
    ASSERT_FALSE(result);

    ASSERT_TRUE( gc->NewString("123")->NotMatch( &context, v , &result ));
    ASSERT_TRUE( result );
  }

  // Comparison operator
#define XX(OP,COP,LHS,RHS) \
  do { \
    Value lhs( gc->NewString(LHS) ); \
    Value rhs( gc->NewString(RHS) ); \
    bool result; \
    lhs.OP(NULL,rhs,&result); \
    ASSERT_EQ(std::string(LHS) COP std::string(RHS),result); \
    ASSERT_EQ(*(gc->NewString(LHS)) COP *(gc->NewString(RHS)),result); \
    ASSERT_EQ(*(gc->NewString(LHS)) COP RHS,result); \
    ASSERT_EQ(*(gc->NewString(LHS)) COP std::string(RHS),result); \
  } while(false)

  XX(Less,<,"hello","Hello");
  XX(LessEqual,<=,"HelloWorld","___world");
  XX(Greater,>,"","__");
  XX(GreaterEqual,>=,"xx","><");
  XX(Equal,==,"xx","__");
  XX(NotEqual,!=,"xx__","><");

#undef XX // XX
}

TEST(VCL,List) {
  Context context(ContextOption() , boost::shared_ptr<CompiledCode>(
        new CompiledCode(NULL)));
  ContextGC* gc = context.gc();
  {
    List* l = gc->NewList(0);
    ASSERT_TRUE( l->empty() );
    ASSERT_EQ(0,l->size());
    l->Push(Value(0));
    l->Push(Value(1));
    l->Push(Value(2));
    ASSERT_EQ(3,l->size());
    ASSERT_FALSE(l->empty());
    ASSERT_EQ(2,l->Index(2).GetInteger());
    ASSERT_EQ(1,l->Index(1).GetInteger());
    ASSERT_EQ(0,l->Index(0).GetInteger());
    l->Pop(); ASSERT_EQ(2,l->size());
    l->Pop(); ASSERT_EQ(1,l->size());
    l->Pop(); ASSERT_EQ(0,l->size());
  }
  {
    List* l = gc->NewList(0);
    l->Push(Value(1));
    {
      Value v;
      ASSERT_TRUE( l->GetIndex(NULL,Value(0),&v) );
      ASSERT_TRUE(v.IsInteger());
      ASSERT_EQ(1,v.GetInteger());
    }
    {
      Value v;
      ASSERT_TRUE( l->SetIndex(NULL,Value(0),Value(100)) );
      ASSERT_FALSE(l->SetIndex(NULL,Value(1),Value(10000)));
      ASSERT_TRUE( Value(l).GetIndex(NULL,Value(0),&v) );
      ASSERT_TRUE( v.IsInteger() );
      ASSERT_EQ( 100 , v.GetInteger() );
    }
    l->Clear();
    ASSERT_TRUE( l->empty() );
  }
}

TEST(VCL,Dict) {
  Context context(ContextOption(),boost::shared_ptr<CompiledCode>(
        new CompiledCode(NULL)));
  ContextGC* gc = context.gc();
  {
    Dict* d = gc->NewDict();
    ASSERT_TRUE( d->empty() );
    ASSERT_EQ(0,d->size());
    ASSERT_TRUE( d->Insert( *gc->NewString("Key1") , Value(0) ) );
    ASSERT_TRUE( d->Insert( *gc->NewString("Key2") , Value(1) ) );
    ASSERT_TRUE( d->Insert( *gc->NewString("Key3") , Value(2) ) );
    ASSERT_TRUE( d->Insert( *gc->NewString("Key4") , Value(3) ) );
    ASSERT_TRUE( d->Insert( *gc->NewString("Key5") , Value(4) ) );

    Value v;
    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key1"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(0,v.GetInteger());

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key2"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(1,v.GetInteger());

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key3"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(2,v.GetInteger());

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key4"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(3,v.GetInteger());

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key5"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(4,v.GetInteger());

    ASSERT_EQ(5,d->size());
    ASSERT_FALSE(d->empty());
  }
  {
    Dict* d = gc->NewDict();
    ASSERT_TRUE( d->empty() );
    ASSERT_EQ(0,d->size());

    ASSERT_TRUE( d->SetProperty(&context,*gc->NewString("Key1"),Value(1)) );
    ASSERT_TRUE( d->SetProperty(&context,*gc->NewString("Key2"),Value(2)) );
    ASSERT_TRUE( d->SetProperty(&context,*gc->NewString("Key3"),Value(3)) );
    ASSERT_TRUE( d->SetProperty(&context,*gc->NewString("Key4"),Value(4)) );
    ASSERT_TRUE( d->SetProperty(&context,*gc->NewString("Key5"),Value(5)) );

    Value v;
    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key5"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ( 5 , v.GetInteger() );

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key4"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(4,v.GetInteger());

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key3"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(3,v.GetInteger());

    ASSERT_TRUE( d->GetProperty(&context,*gc->NewString("Key5"),&v) );
    ASSERT_TRUE( v.IsInteger() );
    ASSERT_EQ(5,v.GetInteger());
  }
}

TEST(VCL,Module) {
  Context context(ContextOption(),boost::shared_ptr<CompiledCode>(
        new CompiledCode(NULL)));
  ContextGC* gc = context.gc();

  {
    Module* m = gc->NewModule("A new module");
    m->AddProperty( *gc->NewString("__") , Value(1) );
    m->AddProperty( *gc->NewString("A") , Value(2) );
    m->AddProperty( *gc->NewString("B") , Value(3) );

    Value v;
    ASSERT_TRUE( m->FindProperty( *gc->NewString("__") , &v ) );
    ASSERT_EQ(1,v.GetInteger());

    ASSERT_TRUE( m->FindProperty( *gc->NewString("A") , &v ) );
    ASSERT_EQ(2,v.GetInteger());

    ASSERT_TRUE( m->FindProperty( *gc->NewString("B") , &v ) );
    ASSERT_EQ(3,v.GetInteger());
  }
}



} // namespace vcl

int main( int argc , char* argv[] ) {
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
