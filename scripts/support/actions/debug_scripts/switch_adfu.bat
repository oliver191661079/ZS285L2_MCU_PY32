@echo off
setlocal ENABLEDELAYEDEXPANSION

call uart\uart.bat uart\uart_adfu.txt
call gdb\gdb.bat gdb\gdb_adfu.txt
