vcl 4.0;


// =============================================
// Local variable assignment
// =============================================
sub t1 {
  {
    new a1 = "string";
    assert( a1 == "string" );
    set a1 = "xx";
    assert( a1 == "xx" );
    set a1 += "zz";
    assert( a1 == "xxzz" );
  }
  {
    declare a = null;
    assert(a == null);
    set a = [];
    assert(type(a) == "list");
    set a = {};
    assert(type(a) == "dict");
  }
  {
    declare a = "";
    assert( a == "" );
    set a = 10.0;
    assert( a == 10.0 );
    set a += 10.0;
    assert( a == 20.0 );
    set a -= 20.0;
    assert( a == 0.0 );
    set a *= 100000.0;
    assert( a == 0.0 );
    set a /= 1000.0;
    assert( a == 0.0 );
  }
}

sub t2 {
  {
    new a = "xxx";
    unset a;
    assert( a == "" );
  }

  {
    new a = 10.0;
    unset a;
    assert( a == 0.0 );
  }

  {
    new a = 10;
    unset a;
    assert( a == 0 );
  }

  {
    new x = true;
    unset x;
    assert( !x );
  }

  {
    new x = null;
    unset x;
    assert( x == null );
  }
}

sub test {
  t1;
  t2;
}
