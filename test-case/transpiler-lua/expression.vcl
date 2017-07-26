vcl 4.0;

/* ---------------------------------------------------
 * Testing Lua51 Transpiler
 * --------------------------------------------------*/

global a1 = 10;
global a2 = 20.3;
global a3 = "string";
global a4 = null;
global a5 = true;
global a6 = false;

sub test_unary {
  assert( -10 == -10 );
  {
    new a = 10;
    assert( -a == -10 );
  }
  {
    new b = true;
    assert( !!b );
  }
  {
    new c = false;
    assert( !c );
  }
  {
    new d = -100;
    assert( -d == 100 );
  }
}

sub test_primary {
  assert( a1 == 10 );
  assert( a2 == 20.3 );
  assert( a3 == "string" );
  assert( a4 == null );
  assert( a5 == true );
  assert( a6 == false );
  {
    new a = [1,2,3,4,5];
    assert( a[0] == 1 && a[1] == 2 && a[2] == 3 && a[3] == 4 && a[4] == 5 );
  }
  {
    new a = { "a" : 1 , "b" : 2 , "c" : [1,2,3,4,5] , "d": true , "e": false , "f": null };
    assert( a.a == 1 );
    assert( a.b == 2 );
    assert( a.c[1] == 2 );
    assert( type(a.c) == "list" );
    assert( type(a) == "dict" );
    assert( a.d == true );
    assert( a.e == false );
    assert( a.f == null );
  }
  {
     new a = { a : 1 , b : 2 , c : 3 , d : 4 , f : 5 };
     assert( a["a"] == 1 );
     assert( a["b"] == 2 );
     assert( a["c"] == 3 );
     assert( a["d"] == 4 );
     assert( a["f"] == 5 );
  }
  {
    new a = { "X-A" : 1 , "Y-F" : 2 };
    assert( a.X-A == 1 );
    assert( a.Y-F == 2 );
  }


  // ------------------------------------------
  // Testing attribute settings and other stuff
  // ------------------------------------------

  {
    new a = { "A" : "a=bcdef;b=cdefg;c=0.9" };
    assert( a.A:a == "bcdef" );
    assert( a.A:b == "cdefg" );
    assert( a.A:c == "0.9" );
  }
}

sub test_binary {
  assert( 1 + 2 == 3 );
  {
    new a = 1;
    assert( a + 1 == 2 );
    assert( a - 1 == 0 );
    assert( a * 2 == 2 );
    assert( a / 1 == 1 );
    assert( a % 1 == 0 );

    new b = 20;
    assert( a < b );
    assert( a > 0 );
    assert( a == 1 );
    assert( a != 2 );
    assert( a <= 1 );
    assert( a <= 2 );
    assert( a >= 1 );
    assert( a >= 0 );
  }
  {
    new b = 2.0;
    assert( 2 + b == 4.0 );
    assert( 2 - b == 0.0 );
    assert( 2 * b == 4.0 );
    assert( 2 / b == 1.0 );

    assert( b == 2.0 );
    assert( b != 1.0 );
    assert( b > 1.0 );
    assert( b >= 2.0 );
    assert( b < 3.0 );
    assert( b <= 2.0 );
  }
  {
    new str = "str";
    assert( str + "xxx" == "strxxx" );
    assert( "xxx"+str == "xxxstr" );
    assert( str >= "Str" );
    assert( str < "zzz" );
    assert( str <= "strxxx");

    assert( str ~ "str" );
    assert( str ~ "[a-zA-Z]*");
    assert( str !~ "123" );
    assert( str !~ "[0-9]+");
  }

  {
    /* string interpolation */
    new a = 2;
    new b = 20;
    new c = true;
    new d = false;
    assert( '${a} + ${b} + ${c} + ${d}' == "2 + 20 + true + false" );
    assert( '${a + b} == 22' == "22 == 22" );
  }
}

sub test_logic {
  {
    new a = true;
    new b = false;
    assert( (a || b) == true );
    assert( (a && b) == false );
    assert( (a && (30)));
    assert( (!a|| 20));
  }
}

sub test_ternary {
  {
    new a = if(true,1,0);
    assert( a == 1 );
  }
  {
    new a = 10;
    assert( if( 10 < a , -1 , if( 10 == a , 0 , -1 ) ) == 0 );
  }
}

sub test_dict {
  {
    new a = {};
    assert( type(a) == "dict" );
  }
  {
    new a = {
      b : 1,
      cccc : 2,
      x_f : 3,
      "x-f" : 4
    };

    assert( a.b == 1 );
    assert( a."cccc" == 2 );
    assert( a."x_f" == 3 );
    assert( a.x-f == 4 );
  }
}

sub test_array {
  {
    new a = [1,2,3,4,5,6];
    assert( a[0] == 1 );
    assert( a[1] == 2 );
    assert( a[2] == 3 );
    assert( a[3] == 4 );
    assert( a[4] == 5 );
    assert( a[5] == 6 );
  }

}

sub test {
  test_unary;
  test_primary;
  test_binary;
  test_logic;
  test_ternary;
  test_dict;
  test_array;
}
