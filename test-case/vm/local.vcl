vcl 4.0;


sub t1 {
  new a = 10;
  assert( a == 10 );
  {
    new b = "string";
    new c = [];
    new d = { "xx" : c };
    assert(a == 10);
    if( true ) {
      new e = [];
      {
        new f = "xxxx"; new v = {};
        {
          new w = "xxxxxx";
            {
              assert(a == 10);
              {
                new a = 1000;
                assert(a == 1000);
              }
              assert(a == 10);
          }
        }
        assert(f == "xxxx");
      }
    }
    assert(b == "string");
    assert(a == 10);
  }
  assert(a == 10);
}

sub t2 {
  new a = 20;
  assert( a == 20 );
  {
    new a = 30;
    assert( a == 30 );
    {
      new a = 40;
      assert( a == 40 );
      {
        new a = 50;
        assert( a == 50 );
      }
      assert(a == 40);
    }
    assert(a == 30);
  }
  assert(a == 20);
}

sub t3 {
  new a = "string";
  assert( a == "string" );
  {
    new a = "axxx";
    assert( a == "axxx" );
    if( true ) {
      new a = "bxxx";
      assert( a == "bxxx" );
      if( true ) {
        new a = "cxxx";
        assert( a == "cxxx" );
      }
      assert( a == "bxxx" );
    }
    assert( a == "axxx" );
  }
  assert( a == "string" );
}

sub calc {
  new a = 1;
  {
    new b = 2;
    new c = 3;
    new d = 4;
    new e = 5;
    set a += 100;
    assert( a == 101 );
    set b += 100;
    assert( b == 102 );
    set c += 100;
    assert( c == 103 );
    set d += 100;
    assert( d == 104 );
  }
  assert(a == 101);
}


sub test {
  t1;
  t2;
  t3;
  calc;
}
