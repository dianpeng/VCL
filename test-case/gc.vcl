vcl 4.0;


/* Trying to do some simple stress test against our simple stop the world GC.
 *
 * We just try to create some very heavy GC workload to see whether our GC
 * trigger works well or not. For a simple stop-the-world GC, a good GC trigger
 * is very critical */

sub lots_of_gc_trigger(size) {
  declare start = time.now_in_micro_seconds();

  for ( _  : loop( 1 , size , 1 ) ) {
    new l = {};
    new r = "string";
    new g = [];
    new s = sub {};
  }

  declare end = time.now_in_micro_seconds();

  println('Time : ${end-start}');

  println('GC running time: ${gc.gc_times()}');
  println('GC size        : ${gc.gc_size()} ');
  println('GC trigger     : ${gc.gc_trigger()}');
  println('GC ratio       : ${gc.gc_ratio()}');

  gc.force_collect();

  println('GC running time: ${gc.gc_times()}');
  println('GC size        : ${gc.gc_size()} ');
  println('GC trigger     : ${gc.gc_trigger()}');
  println('GC ratio       : ${gc.gc_ratio()}');
}

sub lots_of_gc_miss(size) {
  declare collector = [[]];
  declare index = 0;
  declare start = time.now_in_micro_seconds();

  for ( _  : loop( 1 , size , 1 ) ) {
    new l = {};
    new r = "string";
    new g = [];
    new s = sub {};

    if( list.max_size(collector[index]) > list.size(collector[index]) + 8 ) {
      list.push(collector[index],l);
      list.push(collector[index],r);
      list.push(collector[index],g);
      list.push(collector[index],s);
    } else {
      set index += 1;
      list.push(collector,[]);
      list.push(collector[index],l);
      list.push(collector[index],r);
      list.push(collector[index],g);
      list.push(collector[index],s);
    }
  }

  declare end = time.now_in_micro_seconds();

  println('Time : ${end-start}');

  println('GC running time: ${gc.gc_times()}');
  println('GC size        : ${gc.gc_size()} ');
  println('GC trigger     : ${gc.gc_trigger()}');
  println('GC ratio       : ${gc.gc_ratio()}');

  gc.force_collect();

  println('GC running time: ${gc.gc_times()}');
  println('GC size        : ${gc.gc_size()} ');
  println('GC trigger     : ${gc.gc_trigger()}');
  println('GC ratio       : ${gc.gc_ratio()}');

  println("Do GC verification");

  set start = time.now_in_micro_seconds();
  for( index , value : collector ) {
    new end = list.size(value) / 4;
    for( i : loop( 1 , end ) ) {
      assert( type(value[i*4]) == "dict" );
      assert( type(value[i*4+1]) == "string" );
      assert( type(value[i*4+2]) == "list" );
      assert( type(value[i*4+3]()) == "null" );
    }
  }
  set end = time.now_in_micro_seconds();
  println('Time : ${ end - start }');
}

sub do_gc_collect {
  gc.force_collect();
  println('GC running time: ${gc.gc_times()}');
  println('GC size        : ${gc.gc_size()} ');
  println('GC trigger     : ${gc.gc_trigger()}');
  println('GC ratio       : ${gc.gc_ratio()}');
}

sub test {
  lots_of_gc_trigger(1000000);
  lots_of_gc_miss   (100000);
  do_gc_collect;
}
