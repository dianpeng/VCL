vcl 4.0;

// Primary expression testing in VCL. Primary expression is the expression
// that doesn't have any numeric operators inside of the expression.
sub dot {
  {
    new a = { "a": { "b" : { "c" : { "d" : { "e" : { "_" : { "f" : { "g" : { "h" : { "_" : 100 } } } } } } } } } };
    assert( a.a.b.c.d.e._.f.g.h._ == 100 );
  }
  {
    new a = { "a-x" : 100 };
    assert( a.a-x == a."a-x" );
    assert( a.a-x == 100 );
  }
  {
    new a = {};
    dict.insert(a,"X",100);
    dict.insert(a,"x-x",1000);
    assert(a.X  == a.x-x / 10);
  }
}

sub index {
  {
    new a = [];
    list.push(a,1);
    list.push(a,"string");
    list.push(a,"value");
    assert( a[0] == 1 );
    assert( a[1] == "string" );
    assert( a[2] == "value" );
  }
  {
    new b = [[[[[[[1]]]]]]];
    assert(b[0][0][0][0][0][0][0] == 1);
  }
}

sub colon {
  {
    new a = { "a-x" : 100 };
    assert( a:a-x == 100 );
    assert( a:a-x == a.a-x );
    assert( a:a-x == a."a-x" );
  }
  {
    new a = { "X-HTTP" : "Value" };
    assert( a:X-HTTP == "Value" );
  }
}

sub test {
  dot;
  index;
  colon;
}
