#!/usr/sbin/dtrace -s

syscall:::entry
/pid == $target/
{
  @calls[ustack(10), probefunc] = count();
}

profile:::tick-1sec
{
  /** print */
  printa(@calls);
  /** clear */
  clear(@calls);
  trunc(@calls, 15);
}


