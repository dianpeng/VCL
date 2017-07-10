vcl 4.0;

global a = 10;
global b = 20;

sub t1 {
  assert( a );
  assert( b );
}

sub foo { return { 10 }; }
sub bar(a) { return { a + 10 }; }

sub t2 {
  assert( foo() == 10 );
  assert( bar(10) == 20 );
}

sub t3 {
  new a = foo;
  new b = bar;

  assert( a() == 10 );
  assert( b(10) == 20 );
}

sub test {
  t1;
  t2;
  t3;
}
