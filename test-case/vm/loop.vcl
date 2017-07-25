vcl 4.0;

// Loop constructs testing , this is part of the language tests

// 1. Trivail loops, no loop controls
sub t1 {
  new a = [1,2,3,4,5];
  new sum = 0;
  for( _ , v : a ) set sum += v;
  assert( sum == 1 + 2 + 3 + 4 + 5 );

  // sanity check for stack consistency
  new b = 10;
  set a = 100;
  set sum = 1000;
  assert(b == 10 && a == 100 && sum == 1000);
}

sub t2 {
  new a = [1,2,3,4,5];
  new sum = 0;
  for( i : a ) set sum += i;
  assert( sum == 0 + 1 + 2 + 3 + 4 );
}

// 2. Scope with explicitly lexical scope
sub t3 {
  new a = [1,2,3,4,5];
  {
    new sum = 0;
    for( _ , v : a ) set sum += v;
    assert(sum == 1+2+3+4+5);
  }
  assert(type(a) == "list");
  {
    set sum = 0;
    new b = [];
    for( _ , v : b ) set sum += v;
    assert( sum == 0 );
  }
  assert(type(a) == "list");
  set a = 10;
  assert(a == 10);
}

// 3. Consequtive loop structure
sub t4_1 {
  new a = [1,2,3];
  new sum = 0;

  for( index : a ) set sum += index ;
  assert( sum == 0 + 1 + 2 );

  set sum = 0;
  for( _ , v : a )  set sum += v;
  assert( sum == 1 + 2 + 3 );

  set b = [1,2,3,4,5,6];
  set sum = 0;
  for( index : b ) set sum += index;
  assert( sum == 0 + 1 + 2 + 3 + 4 + 5 );

  set sum = 0;
  for( _ , v : b ) set sum += v;
  assert( sum == (1+2+3+4+5+6) );

  set a = 10;
  set b = 100;
  new c = 200;
  new d = 300;
  assert( a == 10 );
  assert( b == 100 );
  assert( c == 200 );
  assert( d == 300 );
}

// Loop control for break
sub t4_2 {
  new a = [1,2,3,4,5];
  new sum = 0;
  for( _ , v : a ) {
    if( v >= 4 ) {
      break;
    }
    set sum += v;
  }
  assert( sum == 1 + 2 + 3 );
  new x = 10;
  assert( x == 10 );
  set a = 100;
  assert( a == 100 );
  set sum = 1000;
  assert( sum == 1000 );
}

// Loop control for all variable along the road
sub t4_3 {
  new a = [1,2,3,4,5];
  new sum = 0;
  for( _ , v : a ) {
    new l1 = true;
    if(l1) {
      new l2 = false;
      new l3 = [];
      new l4 = {};
      new l5 = {};
      new l6 = "string";
      if(!l2) {
        new l11 = true;
        new l12 = 1 + 2 + 3 + 4;
        new l13 = 3 * 100;
        if(l11) {
          new a = "string";
          new b = [];
          new c = {};
          break;
        }
      }
    }
    set sum += v;
  }
  assert(type(a) == "list");
  assert( sum == 0 );
  set a = 100;
  assert( a == 100 );
  new b = 1000;
  assert( b == 1000 );
}

sub t5_1 {
  new a = [1,2,3,4,5];
  new sum = 0;
  for( _ , v : a ) {
    if( v % 2 ) {
      continue;
    }
    set sum += v;
  }
  assert( sum == 2 + 4 );
  new x = 10;
  assert( x == 10 );
  set a = 100;
  assert( a == 100 );
  set sum = 1000;
  assert( sum == 1000 );
}

sub t5_2 {
  new a = [1,2,3,4,5];
  new sum = 0;
  for( _ , v : a ) {
    new l1 = true;
    new l2 = "string";
    new l3 = [];
    new l4 = {};
    if(l1) {
      new l21 = true;
      new l22 = false;
      new l23 = [];
      new l24 = {};
      if(!l22) {
        new l31 = true;
        new l32 = false;
        new l33 = "string";
        if(l31) {
          new xxx = "xxx";
          new xxxx= 1;
          continue;
        }
      }
    }
    set sum += v;
  }

  assert( sum == 0 );
  assert(type(a) == "list");
  set a = 100;
  assert( a == 100 );
  new b = 1000;
  assert( b == 1000 );
}

// Dict object
sub dict_1 {
  new a = { "a" : 1 ,
            "b" : 2 ,
            "c" : 3 ,
            "d" : 4 ,
            "e" : "string" };
  for( k , v : a ) {
    println("Key:",k," Val:",v);
  }
}

sub sum_a_dict_for_integer(a) {
  declare sum = 0;
  for( k , v : a ) {
    if(type(v) == "integer") {
      set sum += v;
    } else if(type(v) == "real") {
      set sum += to_integer(v);
    } else continue;
  }
  return {sum};
}

sub dict_2 {
  assert( sum_a_dict_for_integer( { "a":1,"b":2,"c":null,"D":true,"E":false,"VVV":{}} ) == 1+2 );
  assert( sum_a_dict_for_integer( { "a":1.1 ,"b":2.2 } ) == 3 );
}

// Loop object
sub l1 {
  declare sum = 0;
  for( i : loop(1,101) ) {
    set sum += i;
  }
  assert(sum == 5050);

  {
    new counter = 0;
    declare result;
    for( i : loop() ) {
      set counter += 1;
      if( counter >= 5050 ) { set result = i; break; }
    }
    assert( result == 5049 );
  }
}


sub test {
  t1;
  t2;
  t3;
  t4_1;
  t4_2;
  t4_3;
  t5_1;
  t5_2;
  l1;
  dict_2;
}
