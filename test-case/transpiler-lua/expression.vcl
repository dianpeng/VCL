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
}

sub test {
  test_primary;
}
