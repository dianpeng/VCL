vcl 4.0;

global v = 100;
global v1 = 10.0;

sub test {
  {
    new a = 0;
    assert( "a = 0" == 'a = ${a}' );
  }
  {
    declare a = 0;
    declare b = 1;
    assert( "a + b = 1" == 'a + b = ${a+b}' );
  }
  {
    declare x = "string";
    assert('${100 + 10.01}' == "110.01");
  }
}
