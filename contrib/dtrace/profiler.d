#!/usr/sbin/dtrace -s

dtrace::BEGIN
{
}

syscall:::entry
/pid == $1/
{
  @syscalls[probefunc] = count();
}

profile:::tick-1sec
{
  printa(@syscalls);
  foreach(k; @syscalls.keys.sort)
  {
    @syscalls[k] = 0;
  }
}


