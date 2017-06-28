# A stand alone VCL (Varnish Configuration Language) Virtual Machine and Environment
1. This a *rewrite and redesign* of VCL
2. This engine targets at Varnish 4.0 syntax and also most Fastly extension
3. Multiple extension to language has been added and also now it is a weak/dynamic type lang.


# The API is not stable in current status , it is subject to change ~~~~~~~
Still unstable and anything is subject to change and ACL feature needs to be refactoried and also not properly
test.


# Dependency
1. PCRE
2. boost
3. glog


# Features

### Language feature extension to original VCL and fastly VCL
1. *Weak* type scripting language. Example:
  ```
    declare my_var = 1;
    set my_var = "string";
    println(my_var);
  ```

2. First order function. Example:
  ```
    sub my_foo {
      do_job;
      do_his_job;
    }

    sub return_my_foo {
      declare local_variable = my_foo; // Assign subroutine to a variable
      return { local_variable };       // Return subroutine as value
    }
  ```

3. Argument allowed in sub routine. Example:
  ```
    sub add(a,b) {
      return { a + b };
    }

    global my_result = add(1,2);
  ```

4. Global variable support. Example:
  ```
    global a = 10;
    global b = "string";

    sub show_global {
      println(a);
      println(b);
    }
  ```

5. Return statement and truely recursive execution support. Example:
  ```
    // Fibnacci computation in recursive way
    sub fib(a) {
      if(a == 0 || a == 1 || a == 2)
        return {a};
      else
        return { fib(a-1)+fib(a-2) };
     }
  ```

  Please be aware, to return from a function, the return value expression
  *must* be wrapped with "{" and "}".

  The old *terminate* style return from VCL is still supported with exactly
  same syntax.

  ```
     sub my_sub {
      do_my_job;
      return (purge); # This will terminate the script execution same as VCL
     }
  ```

6. Arithmatic expression support. Example:
  ```
    global a = 1 + 2 *3;
    global b = "string" + "string" + to_string(3);
    global c = to_integer(-1 * 2.0 / 3.0) % 4;
  ```

7. Builtin Json sytanx support. Example:
  ```
    global a_list = [];
    global a_list_of_value = [ 1, 2, [] , "string" , null , true ,false ];
    global a_object = {};
    global a_object_of_value = {
      "a" : "string",
      "b" : a_list_of_value,
      [to_string(3+4+5)] : {}
    };
   ```

8. Call as a statement. Example:
  ```
    sub main {
      call foo;
      foo;
      foo();
      // All there calls above are same
    }
 ```

9. Object oriented notation. Example:
  ```
    sub main {
      declare l = [];

      list.push(l,10);
      list.push(l,11);

      assert(l[0] == 10);
      assert(l[1] == 11);
      // Force Garbage Collection
      gc.force_collect();
    }
  ```

10. Extension literal . Example:
  
  In VCL 4.0 they introduces a new syntax to configure their backend and
  backend group using a C99 intialization list syntax. This one is not
  supported by Fastly however we support it and we support it in a more
  general way. Check out APIs to help understand what it means to us.

  ```
    Extension my_ext = {
      .hostname = "string";
      .value = 1;
      .shift = 2;
    };

    // Extension literal syntax support
    global my_ext = Extension {
      .hostname = "string";
      .value = 1;
    };

    global my_backend = Backend {
      .hostname = "blabla";
      .https = "true";
      .haha = "hehe";
    };
  ```

11. Extended attribute/property access syntax. Example:
  ```
    global a = l[0]; // A indexing access
    global b = l.dot; // A dot access
    global c = l:attr; // An attribute sub component access , supported by Fastly
                       // but not official VCL syntax.
    global d = l."this is a syntax suger"; // If a key is space delimited or not able
                                           // to be written in variable name, then you
                                           // could just use quoted string to express the
                                           // property/attribute name.
 ```
12. Anonymous sub routine definition. Example:
  ```
    global my_foo = sub {
      return {10};
    };

    sub a_sub_return_a_sub {
      declare a_local_sub = sub {
        return {100};
      };
      return {a_local_sub};
    }
  ```

13. Explicit lexical scope. Example:
  ```
    sub my_foo {
      declare l1 = 10;
      {
        declare l1 = 20;
        {
          declare l3 = 30;
          assert( l3 == 30 );
        }
        assert(l3 == 20);
      }
      assert(l3 == 10);
    }
  ```

14. For loop. Example:
  ```
    sub sum(a) {
      new ret = 0;
      for( index , value : a ) {
        set ret += value;
      }
      return {ret};
    }
  ```
For loop can be turned off by configuring the engine.

15. String interpolation. Example:
  ```
    global a = 0;
    global b = 1;
    global string = 'a + b = ${a + b}';

    sub my_foo {
      println( 'This is Hello ${"W" + "O" + "R" + "L" + "D"}' );
    }
  ```
Support expression level interpolation
        

### Syntax difference from Fastly

1. Fastly supports *goto* statement, but I doesn't support it

2. Fastly local variable definition syntax is as following :

```
  declare local var.VariableName Type;
```

In this VCL implementation , you can define your variable via syntax:

```
  declare my_var; // Define a my_var initialized as null
  declare my_var = 10; // Define a my_var initialized as integer 10
```

Basically this VCL implementation uses a *weak* type mode, so no need to
define the type and also no need to prefix your variable with "var". Lastly
no need to add a "local" keyword to define your local variable. Additionally,
we support initialization for local variable.

There's another statement/keyword new which is supported in Varnish VCL 4.0 however
fastly doesn't support it. The new is basically alias from declare in this VCL
implementation. We support new keyword to make sure we are compatible with official
VCL 4.0 grammar.

```
  new my_var = 1000;
  new my_var; // This is not allowed , new must follow a initialization
```

3. Fastly/VCL supports dash to be valid character in variable name. In Fastly or VCL , they support
"-" in their variable name. However this is impossible for a grammar that supports
arithmatic , since it *confuses* the parser to parse a-b to be 1) a variable name
"a-b" or 2) an expression a minus b. To compensate this stuff , since in header ,
we commonly have dash as part of the name , we allow dash as a variable name when
it is part of a index/attribute/property access. But not the leading variable name.

Example:

```
  new my-var-name = 100; // This is wrong, it won't compile
  new value = resp.header.X-Ec-Value; // This is fine, the X-Ec-Value will be treated as
                                     // a variable name.

  new value = resp.header:X-EC-Value; // Fastly sub component access, not valid VCL syntax,
                                     // but fastly has it and we do support it as well.
```

### A library not a application
1. Designed to be used by multiple isolated environment, each environment has its isolated GC and heap , global variables
   and other stuff. It is easy to be used by multi-user or CDN like web server.

2. Preemptable virtual machine. User is able to preempt VM at any time. It is easy to integrate it with timer signal.

3. Rich APIs for extending the runtime. The runtime knows nothing about the detail but simply works on top of a type system.


# Detail
TODO::
