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
    assert('${x +"h"}' == "stringh");
  }

  new sub1 = sub { return {1}; };
  assert( '${sub1() == 1}' == "true" );

  {
    new a = 0;
    new b = 1;
    assert( '${a} == 0 '
            '${b} == 1' == "a == 0 b == 1" );
  }


  /* Constant Fold for String Interpolation */
  {
    new a = 'abcdefg'; // This must be a string
    new b = 'abcdefg' 'bcdefgh' '${a}';
    new c = 'abcdefg' '${b}' 'bcdef' 'bcdef' 'ggg';

    new html_page = '<body> Hello World </body>'
                    '<body> This is a ${a} </body>'
                    '<another> This is a ${a} </another>';
  }
}
