def exit_handler (event):
    code = 1
    if hasattr(event, "exit_code"):
        code = event.exit_code
    with open("exit.out.txt", 'w') as f:
        f.write("{}".format(code))

def crash_handler (event):
  if (isinstance(event, gdb.SignalEvent)):
    log_file_name = "crash.out.txt"
    gdb.execute("set logging file " + log_file_name )
    gdb.execute("set logging on")
    gdb.execute("set logging redirect on")
    gdb.execute("thread apply all bt")
    gdb.execute("q")

gdb.events.stop.connect(crash_handler)
        
gdb.events.exited.connect(exit_handler)
gdb.execute("set confirm off")
gdb.execute("set pagination off")
gdb.execute("r")
gdb.execute("q")
