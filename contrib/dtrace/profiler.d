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
  printa("%@8u %a\n", @syscalls);
}


