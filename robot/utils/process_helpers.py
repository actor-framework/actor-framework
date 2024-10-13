from robot.libraries.BuiltIn import BuiltIn
from robot.api.deco import keyword

@keyword
def write_to_process_stdin(process_handle, input_text):
    process_lib = BuiltIn().get_library_instance('Process')
    process = process_lib.get_process_object(process_handle)
    if process:
        process.stdin.write(input_text.encode())
        process.stdin.flush()
    else:
        raise ValueError(f"Process not found.")
