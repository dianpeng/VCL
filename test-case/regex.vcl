vcl 4.0;

sub match_true( needle , pattern ) {
  if(needle ~ pattern)
    assert(true);
  else
    assert(false);
}

sub match_false( needle , pattern ) {
  if(needle !~ pattern)
    assert(true);
  else
    assert(false);
}

sub t1 {
  match_true("AA","^AA$");
  match_true("BBC","B*C");
  match_false("BBC","ABC");
  match_false("AA","A{3}");
  match_true("AA","A{2}");
}

sub test {
  t1;
}
