vcl 4.0;

sub tscope {
  new l1 = 10;
  {
    new l1 = 100;
    {
      new l1 = 1000;
      {
        new l1 = 10000;
        assert(l1 == 10000);
      }
      assert(l1 == 1000);
    }
    assert(l1 == 100);
  }
  assert(l1 == 10);
}

sub test {
  tscope;
  {
    assert( 1 == 1 );
  }
  {
    {
      {
        {
          {
            {
              assert(true);
            }
          }
        }
      }
    }
  }
}
