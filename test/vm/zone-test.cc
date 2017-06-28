#include <vm/zone.h>
#include <gtest/gtest.h>
#include <cstdlib>

namespace vcl {
namespace vm  {
namespace zone{

TEST(Zone,Basic) {
  {
    Zone zone(1);
    int* ptr[1024];
    size_t sum = 0;
    for( size_t i = 0 ; i < 1024 ; ++i ) {
      ptr[i] = zone.Malloc<int*>(sizeof(int));
      *(ptr[i]) = i;
      sum += i;
    }

    size_t esum = 0;
    for( size_t i = 0 ; i < 1024 ; ++i ) {
      esum += *ptr[i];
    }
    ASSERT_EQ(esum,sum);
    ASSERT_EQ(zone.total_size(),1024*sizeof(int));
    zone.Clear();

    ASSERT_EQ(zone.total_size(),0);
    ASSERT_EQ(zone.total_segment_size(),0);

    // Do the allocation again
    esum = 0;
    for( size_t i = 0 ; i < 1024 ; ++i ) {
      ptr[i] = zone.Malloc<int*>(sizeof(int));
      *(ptr[i]) = i;
    }
    for( size_t i = 0 ; i < 1024 ; ++i ) {
      esum += *ptr[i];
    }
    ASSERT_EQ(esum,sum);
    ASSERT_EQ(zone.total_size(),1024*sizeof(int));
  }

  {
    Zone zone(4); // 4 == sizeof(int)
    ASSERT_EQ(zone.total_size(),0);
    ASSERT_EQ(zone.total_segment_size(),4);
    ASSERT_EQ(zone.size(),4);
    int* ptr = zone.Malloc<int*>(sizeof(int));
    *ptr = 1000;
    ASSERT_EQ(zone.total_size(),4);
    ASSERT_EQ(zone.total_segment_size(),4);
    ASSERT_EQ(zone.size(),0);
    int* ptr2= zone.Malloc<int*>(sizeof(int));
    ASSERT_EQ(zone.total_size(),8);
    ASSERT_EQ(zone.total_segment_size(),12);
    ASSERT_EQ(zone.size(),4);
    *ptr2 = 10;
    int* ptr3 = zone.Malloc<int*>(sizeof(int));
    ASSERT_EQ(zone.total_size(),12);
    ASSERT_EQ(zone.total_segment_size(),12);
    ASSERT_EQ(zone.size(),0);
    *ptr3 = 3;
    ASSERT_EQ(*ptr,1000);
    ASSERT_EQ(*ptr2,10);
    ASSERT_EQ(*ptr3,3);
  }

  // Reallocate APIs
  {
    Zone zone(4);
    char* buf = zone.Malloc<char*>(8);
    ASSERT_EQ(zone.total_size(),8);
    ASSERT_EQ(zone.total_segment_size(),8+4);
    strcpy(buf,"Hello");

    char* nbuf = zone.Realloc<char*>(buf,8,7);
    ASSERT_EQ(nbuf,buf);
    ASSERT_TRUE(strcmp(nbuf,"Hello")==0);
    ASSERT_EQ(zone.total_size(),8);
    ASSERT_EQ(zone.total_segment_size(),8+4);

    nbuf = zone.Realloc<char*>(buf,8,1000);
    ASSERT_NE(nbuf,buf);
    ASSERT_TRUE(strcmp(nbuf,"Hello") ==0);

    ASSERT_EQ(zone.total_size(),8+1000);
    ASSERT_EQ(zone.total_segment_size(),8+4+16+1000);
  }
}

namespace {

std::string RandStr( size_t length ) {
  std::string buf;
  buf.reserve(length);

  for( size_t i = 0 ; i < length ; ++i ) {
    buf.push_back( static_cast<char>(rand() % 128) );
  }
  return buf;
}

} // namespace

TEST(Zone,String) {
  {
    ZoneString zone_string; // Empty string
    ASSERT_EQ(zone_string.size(),0);
    ASSERT_TRUE(zone_string.empty());
    ASSERT_TRUE(strcmp(zone_string.data(),"")==0);
    ASSERT_TRUE(zone_string.ToStdString() == std::string());
    ASSERT_TRUE(zone_string == "");
    ASSERT_TRUE(zone_string == ZoneString());
    ASSERT_TRUE(zone_string  != "a");
    ASSERT_TRUE(zone_string == std::string());
  }
  {
    Zone zone(1);
    ZoneString str(&zone,"ABCD");
    ZoneString* hstr = ZoneString::New(&zone,"ABCD");
    ASSERT_TRUE(str == *hstr);
    ASSERT_TRUE(str == "ABCD");
    ASSERT_TRUE(*hstr == "ABCD");
    ASSERT_TRUE(*hstr == std::string("ABCD"));
    ASSERT_EQ(4,hstr->size());
    ASSERT_EQ(4,str.size());

    for( size_t i = 0 ; i < 1024 ; ++i ) {
      std::string str = RandStr(i + 1);
      ZoneString* s = ZoneString::New(&zone,str);
      ASSERT_EQ(s->size(),i+1);
      ASSERT_FALSE(s->empty());
      ASSERT_EQ(*s,str);
      ASSERT_EQ(*s,str.c_str());
      ASSERT_EQ(s->size(),str.size());
    }
  }
  {
    Zone zone(1);
    ZoneString empty;
    ZoneString* hempty = ZoneString::New(&zone);
    ASSERT_TRUE(empty == *hempty);
    ASSERT_TRUE(empty.data() == ZoneString::kEmptyCStr);
    ASSERT_TRUE(hempty->data() == ZoneString::kEmptyCStr);
    ASSERT_EQ(empty.size(),0);
    ASSERT_EQ(hempty->size(),0);
  }
  {
    Zone zone(1);
    ZoneString str1(&zone,"A");
    ZoneString*strb = ZoneString::New(&zone,"BCdefg");
    ASSERT_TRUE(str1 != *strb);
    ASSERT_TRUE(str1 <  *strb);
    ASSERT_TRUE(str1 <= *strb);
    ASSERT_TRUE(*strb>  str1 );
    ASSERT_TRUE(*strb>= str1 );
    ZoneString* another_str = ZoneString::New(&zone,"BCdefg");
    ASSERT_TRUE(*strb == *another_str);
    ASSERT_TRUE(*strb >= *another_str);
    ASSERT_TRUE(*strb <= *another_str);
    ASSERT_FALSE(*strb < *another_str);
    ASSERT_FALSE(*strb > *another_str);
  }
}

TEST(ZoneVector,Vector) {
  {
    Zone zone(1);
    ZoneVector<int> vector;
    int sum = 0;
    for( int i = 0 ; i < 1024 ; ++i ) {
      vector.Add(&zone,i);
      sum += i;
    }

    int esum = 0;
    for( int i = 0 ; i < 1024 ; ++i ) {
      esum += vector[i];
    }

    ASSERT_EQ(esum,sum);
    ASSERT_EQ(vector.size(),1024);
    ASSERT_FALSE(vector.empty());

    ASSERT_TRUE(vector.First() == 0);
    ASSERT_TRUE(vector.Last() == 1023);
  }
  {
    Zone zone(1);
    ZoneVector<int> vector;
    vector.Reserve( &zone , 1024 );
    ASSERT_EQ(vector.size(),0);
    ASSERT_TRUE(vector.empty());
    ASSERT_EQ(vector.capacity(),1024);
    for( int i = 0 ; i < 1024 ; ++i ) {
      vector.Add(&zone,i);
    }

    ASSERT_EQ(vector.capacity(),1024);
    ASSERT_EQ(vector.size(),1024);
    ASSERT_FALSE(vector.empty());

    int sum = 0;
    int esum= 0;
    for( int i = 0 ; i < 1024 ; ++i ) {
      sum += i;
      esum += vector[i];
    }

    ASSERT_EQ(sum,esum);
  }
  {
    Zone zone(1);
    ZoneVector<int> vector;
    vector.Reserve(&zone,1024);
    for( int i = 0 ; i < 1000 ; ++i ) {
      vector.Add(&zone,i);
    }
    ASSERT_EQ(vector.size(),1000);
    ASSERT_EQ(vector.capacity(),1024);
    for( int i = 0 ; i < 1000 ; ++i ) {
      vector.Pop();
    }
    ASSERT_EQ(vector.size(),0);
    ASSERT_EQ(vector.capacity(),1024);
    for( int i = 0 ; i < 1000 ; ++i ) {
      vector.Add(&zone,i);
    }
    ASSERT_EQ(vector.size(),1000);
    ASSERT_EQ(vector.capacity(),1024);
    vector.Clear();
    ASSERT_EQ(vector.size(),0);
    ASSERT_EQ(vector.capacity(),1024);
  }
  {
    Zone zone(1);
    ZoneVector<int> vector;
    vector.Resize(&zone,1024);
    for( int i = 0 ; i < 1024 ; ++i ) {
      ASSERT_EQ(vector.Index(i),0);
    }
  }
  {
    Zone zone(1);
    ZoneVector<int> vector(&zone,1024);
    ASSERT_EQ(vector.capacity(),1024);
    ASSERT_EQ(vector.size(),0);
    vector.Add(&zone,1);
    ASSERT_EQ(vector.First(),1);
    ASSERT_EQ(vector.size(),1);
    ASSERT_EQ(vector.capacity(),1024);
  }
  {
    Zone zone(1);
    ZoneVector<int> vector(&zone,1024,2048);
    ASSERT_EQ(vector.capacity(),2048);
    ASSERT_EQ(vector.size(),1024);
    for( size_t i = 0 ; i < 1024 ; ++i ) {
      ASSERT_EQ(vector[i],0);
    }
    vector.Add(&zone,1);
    ASSERT_EQ(vector.size(),1025);
    ASSERT_EQ(vector.Last(),1);
    ASSERT_EQ(vector.capacity(),2048);
  }
  {
    Zone zone(1);
    ZoneVector<int>* hvec = ZoneVector<int>::New(&zone);
    ASSERT_EQ(hvec->size(),0);
    ASSERT_EQ(hvec->capacity(),0);
    hvec->Add(&zone,1000);
    ASSERT_EQ(hvec->First(),1000);
    ASSERT_EQ(hvec->capacity(),2);
  }
  {
    Zone zone(1);
    ZoneVector<int>* hvec = ZoneVector<int>::New(&zone,1024);
    ASSERT_EQ(hvec->size(),0);
    ASSERT_EQ(hvec->capacity(),1024);
    hvec->Add(&zone,1000);
    ASSERT_EQ(hvec->size(),1);
    ASSERT_EQ(hvec->capacity(),1024);
  }
}


} // namespace vcl
} // namespace vm
} // namespace zone

int main( int argc , char* argv[] ) {
  srand(0);
  ::testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
