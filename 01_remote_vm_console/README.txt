01_redirected_input_output -- Stage I.
Здесь реализовано консольное приложение, которое порождает дочерний процесс (CMD.EXE) и связывает 2 pipами оба процесса:
Parent умеет писать в -> Child' STDIN
Parent умеет читать из <- Child's STDOUT.

02_client_server_with_child_process -- Stage II.
См. соответствующий README в этой директории