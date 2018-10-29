#!/usr/sbin/dtrace -s


ulong[string] calls;

dtrace::BEGIN
{
}

syscall:::entry
/pid == $1/
{
  calls[probefunc] = count();
}

profile:::tick-1sec
{
  printa(calls);
  foreach(k, _; calls)
  {
    @syscalls[k] = 0;
  }
}


