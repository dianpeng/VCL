vcl 4.0;

/* Here we show a simple of object oriented programming way to do work */
sub new_object(init_var) {
  declare ret = {
    my_foo : sub(self,b) {
      return { self.my_var + b };
    },

    my_var : init_var
  };
  return {ret};
}

sub t1 {
  new object = new_object(10);
  assert( object::my_foo(10) == 20 );
}

/* ---------------------------------------------------
 * Testing nested sub_routine's stack consistency
 * -------------------------------------------------*/
global obj = {
  a : { b : { c : sub(self,arg1) { return {self.my_arg + arg1}; } , my_arg : 100 } }
};

sub t2 {
  declare a = 10;
  assert( a + 10 == 20 );
  if(true) {
    declare b = 20;
    assert(b * 2 == 40);
    assert( obj.a.b::c(100) == 200 );
  }
  declare c = 20;
  assert( c == 20 );

  declare xx = [1,2,3,4,5,6];
  declare sum = 0;


  assert(sum == 12);
}

/* Testing global scope method call */
global xx = obj.a.b::c(100);
global xx2= obj.a.b::c(200);
global xx3= obj.a.b::c(300);

sub t3 {
  declare x = 10;
  assert( x == 10 );
  assert(xx == 200);
  assert(xx2== 300);
  assert(xx3== 400);
  {
    new f = obj.a.b;
    assert( f::c(100) == 200 );
    assert( f::c(200) == 300 );
    assert( f::c(300) == 400 );
  }
}


global obj2 = { a : { b : { c : sub(self) { return {self.my_arg*200}; } , my_arg : 2 } } };

sub t4 {
  assert( obj2.a.b::c() == 400 );
}

sub test {
  t1;
  t2;
  t3;
}
