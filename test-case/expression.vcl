vcl 4.0;

global a = 10;
global a1 = [];
global a2 = "string";
global a3 = "xxxx";
global a4 = { "aaa" : "bbb" + "ccc" , "ccc" : "ddd" + "eee" };

sub test_global {
  assert(a == 10, "a == 10");
  assert(type(a) == "integer");
  assert(type(a1) == "list"  );
  assert(type(a2) == "string");
  assert(type(a3) == "string");
  assert(type(a4) == "dict" );
  assert( a4.aaa == "bbbccc" );
  assert( a4["ccc"] == "dddeee" );
  assert( a2 == "string" );
  assert( a3 == "xxxx" );
}

sub test_aop {
  // ===============================================================
  // Integer
  // ===============================================================
  {
    declare a = 10;
    set a += 10;
    call assert( a == 20 , " a == 20 ");
  }

  {
    declare a = 10;
    set a -= 10;
    call assert( a == 0 , "a == 0");
  }

  {
    declare a = 10;
    set a *= 10;
    call assert( a == 100 , "a == 100");
  }

  {
    declare a = 10;
    set a /= 10;
    call assert( a == 1 , " a == 1 ");
  }

  {
    declare a = 10;
    set a %= 10;
    call assert( a == 0 , " a == 0 ");
  }

  // ==============================================================
  // Double
  // ==============================================================
  {
    declare a = 10.0;
    set a += 10.0;
    call assert( a == 20.0 , "a == 20.0" );
    set a += 10;
    call assert( a == 30.0 , "a == 30.0" );
  }

  {
    declare a = 10.0;
    set a -= 10.0;
    call assert( a == 0.0 , " a == 0.0" );
    set a -= 10;
    call assert( a == -10.0, " a == -10.0" );
  }

  {
    declare a = 10.0;
    set a *= 10.0;
    call assert( a == 100.0 , "a == 100.0" );
    set a *= 1;
    call assert( a == 100.0 , "a == 100.0" );
  }

  {
    declare a = 10.0;
    set a /= 10.0;
    call assert( a == 1.0 , "a == 1.0" );
    set a /= 1.0;
    call assert( a == 1.0 , "a == 1.0" );
  }

  // ========================================================
  // Promotion
  // ========================================================
  {
    declare a = 10;
    set a += 10.0;
    call assert( a == 20.0 , "a == 20.0" );
  }
  {
    declare a = 10;
    set a -= 10.0;
    call assert( a == 0.0 , "a == 0.0");
  }
  {
    declare a = 20;
    set a *= 1.0;
    call assert( a == 20.0, "a == 20.0");
  }
  {
    declare a = 20;
    set a /= 2.0;
    call assert( a == 10.0 , "a == 10.0" );
  }

  {
    declare a = 10.0;
    set a += 10;
    call assert( a == 20.0 , "a == 20.0" );
  }
  {
    declare a = 10.0;
    set a -= 10;
    call assert( a == 0.0 , "a == 0.0" );
  }
  {
    declare a = 20.0;
    set a /= 2;
    call assert( a == 10.0 , "a == 10.0" );
  }
  {
    declare a = 20.0;
    set a *= 1;
    call assert( a == 20.0, "a == 20.0" );
  }

  {
    declare a = true;
    set a += 2;
    call assert( a == 3 , "a == 3" );
  }
  {
    declare a = false;
    set a += 0;
    call assert( a == 0 , "a == 0" );
  }
  {
    declare a = true;
    set a -= 1;
    call assert( a == false, "a == 0");
  }
  {
    declare a = false;
    set a += 1;
    call assert( a == 1 , "a == 1");
  }
  {
    declare a = false;
    set a *= 1;
    call assert( a == 0 , "a == 0");
  }
  {
    declare a = true;
    set a *= 2;
    call assert( a == 2 , "a == 2");
  }
}

sub test_unset {
  {
    declare a = 10;
    unset a;
    call assert(a == 0,"a==0");
  }
  {
    declare a = 10.0;
    unset a;
    call assert(a == 0.0,"a == 0.0");
  }
  {
    declare a = "string";
    unset a;
    call assert(a == "","a == \"\"");
  }
}

sub test_comparison {
  {
    declare a = 10;
    assert( a > 8 , "10 >8");
    assert( a < 20, "10 <20");
    assert( a == 10,"10 == 10");
    assert( a != 0.0,"10 != 0.0");
    assert( a >= 10,"10 >=10");
    assert( a >=  9,"10 >= 9");
    assert( a <=10 ,"10 <=10");
    assert( a <=11 ,"10 <=11");
  }
  {
    declare a = 10.0;
    assert( a > 8.1, "10.0 > 8.1");
    assert( a < 10.1,"10.0 < 10.1");
    assert( a == 10.0,"10.0 == 10.0");
    assert( a != 10.01,"10.0 != 10.01");
    assert( a >= 10.0 , "10.0 >= 10.0");
    assert( a >= 9.9  , "10.0 >= 9.9");
    assert( a <= 10.0 , "10.0 <= 10.0");
    assert( a <= 10.1 , "10.0 <= 10.1");
  }
}

// ======================================================
// List testing
// ======================================================
sub test_list {
  {
    declare l = [];
    assert( list.empty(l) );
    assert( list.size(l) == 0);
    list.push(l,1);
    list.push(l,2);
    list.push(l,"3");
    list.push(l,"4");
    list.push(l,[]);
    assert( list.empty(l) == false );
    assert( list.size(l) == 5 );
    assert( list.index(l,0) == 1 );
    assert( list.index(l,1) == 2 );
    assert( list.index(l,2) == "3" );
    assert( list.index(l,3) == "4" );
    assert( list.empty( l[4] ) );

    list.pop(l);
    assert( list.size(l) == 4 );
    assert( l[3] == "4" );

    list.pop(l); list.pop(l); list.pop(l);
    assert( list.size(l) == 1 );
    assert( l[0] == 1 );
  }

  {
    declare l = [1,2,3,4,5];
    assert( list.empty(l) == false );
    assert( l[0] == list.index(l,0) );
    assert( l[1] == list.index(l,1) );
    list.resize(l,10);
    assert( list.size(l) == 10 );
    assert( l[9] == null );
    assert( l[8] == null );
    assert( l[7] == null );
    assert( l[6] == null );
    assert( l[5] == null );
    assert( l[4] == 5 );
    list.clear(l);
    assert( list.empty(l) );
  }

  {
    declare l = list.range(1,10,1);
    assert( list.size(l) == 9 );
    assert( list.empty(l) == false );
    assert( l[8] == 9 );
    assert( list.front(l) == 1 );
    assert( list.back(l) == 9 );
    declare l2 = list.slice(l,1,2);
    assert( list.size(l2) == 1 );
    assert( l2[0] == 2 );
  }
}

// ======================================================
// Dict testing
// ======================================================

sub test_dict {
  {
    declare a = { "a" : "b" };
    declare b = {};
    declare c = { "a" : { "b" : { "c" : "d" } } };

    assert( dict.size(a) == 1 );
    assert( dict.empty(b) );
    assert( dict.size(c) == 1 );
    assert( dict.size(c.a) == 1 );
    assert( dict.size(c.a.b) == 1 );

    declare x = dict.find(c,"a");
    assert( x.b.c == "d" );
    assert( dict.remove(c,"a") );
    assert( x.b.c == "d" );
    assert( dict.empty(c) );
  }
}

// ======================================================
// Builtin testing
// ======================================================
sub test_builtin {
  {
    new a = 1;
    new b = 2;
    new c = 3;
    declare result = min(a,b,c);
    assert( result == 1 );
  }
  {
    declare l = [1,2,3,4,5,6,7,8,9];
    assert( list.join(l) == 1+2+3+4+5+6+7+8+9 );
  }
  {
    assert( string.upper("aabbcc") == "AABBCC" );
    assert( string.lower("AAAA") == "aaaa" );
    assert( string.size("AAA") == 3 );
    assert( string.empty("") == true );
    assert( string.empty("A") == false );
    assert( string.left_trim("   AAA") == "AAA" );
    assert( string.right_trim("AAA   ") == "AAA" );
    assert( string.trim("   xxx   ") == "xxx" );
    assert( string.dup("Hello World") == "Hello World" );
    assert( string.slice("Hello World",0,5) == "Hello" );
    assert( string.index("Hello",2) == "l" );
    assert( string.slice("Hello World",-1,100) == "Hello World" );
  }
}

sub do_gc {
  gc.force_collect();
}

// A simple fibanacci function using recursion , mainly test recursion
sub fib(a) {
  if(a <= 2) return {a};
  else return {fib(a-1) + fib(a-2)};
}

sub test {
  declare start = time.now_in_micro_seconds();
  assert( fib(4) == 5 , "fib(4) == 5");
  test_dict;
  test_builtin;
  test_aop;
  test_unset;
  test_comparison;
  test_global;
  test_list;
  do_gc();
  declare end = time.now_in_micro_seconds();
}
