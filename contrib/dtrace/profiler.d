#!/usr/sbin/dtrace -s

dtrace::BEGIN
{
}

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


