# Example Invocation:
#
#    gcc test.c -o test -g
#    TRACE_FUNCTIONS=fib TRACE_FILE=log gdb -x gdb_trace.py test
#    cat log
#
#  Log File Format:
#
#    ('call', Parent Call ID, Call ID, Breakpoint Name, Symbol Name, Arguments)
#    ('return', Call Id, Breakpoint Name, Return Value)
#    ('exit', Exit Code)

import gdb
import sys
import os

fd = open(os.environ.get('TRACE_FILE', "/tmp/log"), "w+")
def log(*args):
    fd.write(repr(args) + "\n")

trace_functions = [x.strip() for x in os.environ.get('TRACE_FUNCTIONS', 'main').split(',')]

def exit_handler(event):
    if hasattr(event, 'exit_code'):
        log('exit', event.exit_code)
    else:
        log('exit', None)
gdb.events.exited.connect(exit_handler)

def format_value(var, max_depth=30):
    try:
        str(var) # Invalid references
    except:
        return None

    if var is None or var.is_optimized_out or max_depth == 0:
        return None

    type = var.type.strip_typedefs()

    if type.code in (gdb.TYPE_CODE_INT, gdb.TYPE_CODE_ENUM, gdb.TYPE_CODE_CHAR):
        return {'type': str(var.type), 'value': int(var)}

    if type.code in (gdb.TYPE_CODE_FLT, ):
        return {'type': str(var.type), 'value': float(var)}

    if type.code in (gdb.TYPE_CODE_PTR,):
        # Do not derference types starting with _


        try:
            return {'type': str(var.type), 'value': var.string()}
        except gdb.error:
            pass
        deref = None
        if str(var.type)[0] != "_":
            try:
                deref = var.dereference()
                deref = format_value(deref, max_depth-1)
            except:
                deref = None
        return {'type': str(var.type), 'value': int(var), 'deref': deref}


    if type.code in (gdb.TYPE_CODE_STRUCT,):
        fields = {}
        for field in var.type.fields():
            fields[field.name] = format_value(var[field], max_depth-1)
        return {'type': str(var.type), 'value': fields}

    return {'type': str(var.type), 'value': None}

def break_signals():
    trace_signals = os.environ.get('TRACE_SIGNALS', 'False')
    if trace_signals == 'True':
        signal_register_functions = ['sigaction', 'signal']
        gdb.execute('handle SIGINT nostop pass')

        for fn in signal_register_functions:
            SignalRegisterBreakpoint(fn)


active_calls = []
call_id = 0

class FunctionBreakpoint(gdb.Breakpoint):
    def __init__(self, name):
        gdb.Breakpoint.__init__(self, name)
        self.silent = True
        self.name = name

    def stop(self):
        global call_id
        try:
            frame = gdb.newest_frame()
        except RuntimeError:
            return False
        pframe = frame
        pcall_id = None
        while pframe:
            found = False
            for pname, pcall_id in reversed(active_calls):
                if pname == pframe.name():
                    found = True
                    break
            if found:
                break
            pframe = pframe.older()
            pcall_id = None

        call_id += 1
        active_calls.append([frame.name(), call_id])


        
        try:
            arguments = [x for x in frame.block() if x.is_argument]
            arguments = [(x.name, format_value(frame.read_var(x)))
                         for x in arguments]
            if not arguments:
                raise RuntimeError
        except RuntimeError as e: # Cannot locate block or frame
            output = gdb.execute("info args", False, to_string=True)
            if "No arguments" in output or "No symbol" in output:
                arguments = []
            else:
                lines = output.strip().split("\n")
                arguments = [
                    line.split("=") for line in lines]
                arguments = [(name.strip(), {'type': 'unknown',
                                          'value': value.strip()})
                             for (name, value) in arguments]
        
        thread_id = gdb.selected_thread().global_num
        pcall_id, call_id, self.name, frame, arguments = self.__intercept_call__(pcall_id, call_id, self.name, frame, arguments)
        log('call', pcall_id, call_id, self.name, frame.name(), arguments, thread_id)
        # Break at finish of this function
        if frame.name() != 'main':
            FinishBreakpoint(frame, self.name, call_id)
        else: # main
            for fn in trace_functions:
                FunctionBreakpoint(fn)
        return False

    def __intercept_call__(self, pcall_id, call_id, name, frame, arguments):
        """
        Allows additional logic to operate on the call before it is returned.
        Used for derived classes.
        """
        return (pcall_id, call_id, self.name, frame, arguments)

class SignalRegisterBreakpoint(FunctionBreakpoint):
    """
    Class representing a breakpoint to a function call to register a signal handler.
    """
    def __intercept_call__(self, pcall_id, call_id, name, frame, arguments):
        self.__handle_sigaction__(name, arguments, call_id)
        self.__handle_signal__(name, arguments, call_id)
        
        return (pcall_id, call_id, name, frame, arguments)

    def __handle_sigaction__(self, name, arguments, call_id):
        if name == "sigaction":
            signal_info = gdb.execute("info symbol act->__sigaction_handler.sa_sigaction", to_string=True)
            if signal_info is not None:
                signal_handler_name = signal_info.split(" ")[0]
                signal_bp = SignalFunctionBreakpoint(signal_handler_name, call_id)

    
    def __handle_signal__(self, name, arguments, call_id):
        if name == "signal":
            signal_info = gdb.execute("info symbol handler", to_string=True)
            if signal_info is not None:
                signal_handler_name = signal_info.split(" ")[0]
                SignalFunctionBreakpoint(signal_handler_name, call_id)

class SignalFunctionBreakpoint(FunctionBreakpoint):
    """
    A class representing a breakpoint to a signal function.
    """
    def __init__(self, name, parent_id):
        super().__init__(name)
        self.signal_parent_id = parent_id

    def __intercept_call__(self, pcall_id, call_id, name, frame, arguments):
        # Here we alter ids to allow to create a call graph in which 
        # it seems like a signal registering function such as `sigaction`
        # synchronously called the signal handler function.
        # While this is not true it allows to seamlessly traverse the call graph
        # for signal handlers too.

        return (self.signal_parent_id, call_id, name, frame, arguments)

class FinishBreakpoint (gdb.FinishBreakpoint):
    def __init__(self, frame, name, call_id):
        super().__init__(frame, True)
        self.frame = frame
        self.call_id = call_id
        self.name = name

    def stop (self):
        errno = None
        log('return', self.call_id, self.name,
            format_value(self.return_value),
        )
        active_calls.pop()
        return False # Do not Break

    def out_of_scope (self,):
        log('abort', self.name, self.call_id)

FunctionBreakpoint('main')
break_signals()
gdb.execute('run')
fd.close()
gdb.execute('quit')
