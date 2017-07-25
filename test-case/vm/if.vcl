vcl 4.0;

sub t1 {
  declare a = true;
  declare b = false;
  declare c = 1;
  declare d = 0.0;
  declare e = "string";

  if(a)
    if(!b)
      if(c)
        if(!d)
          if(e) {
            assert(true);
            return;
          }
  assert(false);
}

sub t2 {
  declare a = 10;

  if(a == 0) {
    assert(false);
  } else if(a == 5) {
    assert(false);
  } else if(a == 6) {
    assert(false);
  } else if(a == 7) assert(false);
    else if(a == 8) assert(false);
    else if(a == 9) assert(false);
    else if(a == 10) assert(true);
    else assert(false);
}

sub t3 {
  declare a = 10;
  if( a== 1 ) assert(false);
  else if(a == 2) assert(false);
  else if(a == 3) assert(false);
  else if(a == 4) assert(false);
  else if(a == 5) assert(false);
  else { assert(true); }
}

sub t4 {
  declare a = true;
  if(a) {
    assert(true);
  } else {
    assert(false);
  }
}

sub t5 {
  {
    declare a = false;
    if(a) {
      assert(false);
    } else if(a == 2) {
      assert(false);
    } else {
      assert(true);
    }
  }
  {
    declare a = false;
    if(a) {
      assert(false);
    } else if(!a) {
      assert(true);
    } else {
      assert(false);
    }
  }
}

sub test {
  t1;
  t2;
  t3;
  t4;
  t5;
  if(true) assert(true);
  if(false) assert(false);
}
