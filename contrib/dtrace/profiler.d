#!/usr/sbin/dtrace -s

syscall:::entry
/pid == $1/
{
  @syscalls[probefunc] = count();
}

profile:::tick-1sec
{
  printa(@syscalls);
}


