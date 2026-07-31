@echo off
setlocal ENABLEDELAYEDEXPANSION

set gdb="csky-elfabiv2-gdb.exe"

::--------------------------------------------------------
::-- %1: shell command file
::--------------------------------------------------------
if "%1" == "" (
	@echo Usage:
	@echo     %~nx0 command_file
	@echo Example:
	@echo     %~nx0 gdb_adfu.txt
	goto :eof
)

%gdb% -x %1

