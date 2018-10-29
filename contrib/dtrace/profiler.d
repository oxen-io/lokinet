#!/usr/sbin/dtrace -s

syscall:::entry
/pid == $1/
{
  @calls[probefunc] = count();
}

profile:::tick-1sec
{
  /** print */
  printa(@calls);
  /** clear */
  trunc(@calls);
}


