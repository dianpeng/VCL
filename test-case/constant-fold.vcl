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


sub test {
  t1;
}
