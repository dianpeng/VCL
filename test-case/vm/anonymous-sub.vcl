vcl 4.0;

/* Testing Anonymous Function Call
 *
 * Anonymous function call are speical feature we have here in our VCL implementation,
 * user are allowed to define a function without name and bind it to whatever varaible
 */


global a_foo = sub(a,b,c,d) {
  return {a+b+c+d};
};


global a_bar = sub(a,b,c,d,e) {
  return {a*b*c*d*e};
};

sub t1 {
  assert( a_foo(1,2,3,4) == 1+2+3+4 );
  assert( a_bar(1,2,3,4,5) == 1 * 2 * 3 * 4 * 5 );
}

sub t2 {
  declare foo = sub(a,b,c) { return {a+b+c}; };
  assert( foo(1,2,3) == 6 );
  {
    new a = 10;
    assert( foo(10,10,a) == 30 );
  }
}

sub t3 {
  new result = 100;
  declare foo = sub(a,b,c) {
    declare x = 100;
    return {x + a + b -c};
  };
  assert( foo(10,10,20) == result );
}

sub test {
  t1;
  t2;
  t3;
}
