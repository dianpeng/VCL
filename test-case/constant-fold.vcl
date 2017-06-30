vcl 4.0;

// This file mostly testing the constant-folding pass is correct or not

sub t1 {
  assert( 1 + 1 == 2 );
  assert( 1 - 1 == 0 );
  assert( 1 * 2 == 2 );
  assert( 2 / 2 == 1 );
  assert( 2 % 2 == 0 );
  assert( 1.0 + 2.0 == 3.0 );
  assert( 1.0 - 2.0 == -1.0);
  assert( 1.0 * 2.0 == 2.0 );
  assert( -2.0 / 2.0 == -1.0 );
  assert( "H" + "W" == "HW"  );
  assert( ""  + "XXX" == "XXX");
  assert( "" "XXX" "VVVV" "YY" == "XXXVVVVYY");
}

sub t2 {
  /* Promotion */
  assert( 1 + 1.0 == 2.0 );
  assert( 1 + 1.1 == 2.1 );
  assert( true + 1 == 2 );
  assert( false + 1== 1 );
  assert( true + 2.1 == 3.1 );
  assert( false + 2.2 == 2.2);
}

sub partial {
  {
    new a = 10;
    assert( 1 + 2 + a == 13 );
  }
  {
    new a = 20;
    assert( 1 + 2 + a + 3 + 4 == 30 );
  }
  {
    new a = 30;
    assert( a + 2 + 3 + 4 + 1 == 40 );
  }
}

sub comparison {
  assert( 1 == 1 );
  assert( ( 2 + 1 ) >= 3 );
  assert( ( 2 + 1 ) == 3 );
  assert( 2 < 3 );
  assert( 2 != 1 );
  assert( 2 <= 3 );
  assert( 2 <= 2 );

  assert( 2.1 <= 2.2 );
  assert( 2.1 <  2.2 );
  assert( 2.1 >= 1.9 );
  assert( 2.1 > 1.9 );
  assert( 2.0 >= 2.0  );
  assert( 2.0 != 3.0 );
  assert( 2.0 != 1.0 + 2.0 + 3.0 );
  assert( 2.0 == 1.0 + 1.0 );
  assert( 3.1 == 2.0 + 1.1 );

  assert( true > false );
  assert( true >= false);
  assert( true != false);
  assert( !(true == false) );

  // mixed partially constand folding stuff
  {
    new a = 10.0;
    assert( a + (5.0>= 10.0) == a );
  }
  {
    new a = 20.0;
    assert( (5.0 <= 10.0) + 2 + 3 + a == a + 6 );
  }
}

sub logic {
  assert( true || (a &&b) );
  assert( !(false&& (a ||b)) );
  assert( true || false && b );
  assert( (0 || true) == true);
  {
    new a = true;
    assert( (0 || true) == a );
  }
  {
    new a = 1;
    assert( (1 + 2 + 3) >= a );
  }
}

sub test {
  t1;
  t2;
  partial;
  comparison;
  logic;
}
