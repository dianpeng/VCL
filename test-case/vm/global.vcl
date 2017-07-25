vcl 4.0;

global a = 10;
global b = 20.0;
global c = true;
global d = false;
global e = [];
global f = [1,2,3,4,5];
global g = {};
global h = { "A" : 123 , "B" : 234 };


sub t1 {
  assert( a == 10 );
  assert( b == 20.0 );
  assert( c == true );
  assert( d == false );
  assert( list.empty(e) );
  assert( f[0] == 1 &&
          f[1] == 2 &&
          f[2] == 3 &&
          f[3] == 4 &&
          f[4] == 5 );
  assert( dict.empty(g) );
  assert( h.A == 123 && h.B == 234 );
}

global new_a = 20;
global new_b = 10.0;
global new_c = false;
global new_d = true;
global new_e = [1,2,3,4];
global new_f = [];
global new_g = { "AAA" : "BBB" };
global new_h = "string";


sub t2 {
  set a = new_a;
  set b = new_b;
  set c = new_c;
  set d = new_d;
  set e = new_e;
  set f = new_f;
  set g = new_g;
  set h = new_h;

  assert( a == new_a );
  assert( b == new_b );
  assert( c == new_c );
  assert( d == new_d );
  assert( e == new_e );
  assert( f == new_f );
  assert( g == new_g );
  assert( h == new_h );
}

sub t3 {
  {
    set a += 10;
    assert( a == 30 );
    set a -= 20;
    assert( a == 10 );
    set a *= 2;
    assert( a == 20 );
    set a /= 2;
    assert( a == 10 );
    set a %= 2;
    assert( a == 0 );
  }

  {
    set b += 10.0;
    assert( b == 20.0 );
    set b -= 20.0;
    assert( b == 0.0 );
    set b += -10.0;
    set b *= 2.0;
    assert( b == -20.0 );
    set b /= 2.0;
    assert( b == -10.0 );
  }

  {
    set c = true;
    set c += true;
    assert( c == 2 );
    set c -= 1;
    assert( c == true );
    set c -= 1.0;
    assert( c == 0.0 );
    set c = true;
    set c *= false;
    assert( c == false );
    set c = false;
    set c /= true;
    assert( c == false );
    set c = false;
    set c %= true;
    assert( c == false );
  }

  {
    assert( list.empty(f) );
    list.push( f , 1 );
    list.push( f , 2 );
    assert( list.size(f) == 2 );
    list.pop ( f );
    assert( list.size(f) == 1 );
    assert( f[0] == 1 );
    unset f;
    assert( list.empty(f) );
  }
}

sub test {
  t1;
  t2;
  t3;
}
