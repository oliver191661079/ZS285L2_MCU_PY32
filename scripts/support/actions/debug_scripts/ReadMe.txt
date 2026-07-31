1.cds_debug.gdb.ini文件用于CDS调试的时候加载gdb初始化脚本，使用方法是打开cds调试工程，在debug configurations中debugger加载initial script

The cds_debug.gdb.ini file is used to load the gdb initialization script during CDS debugging. The usage method is to open the cds debugging project and load the initial script in the debugger of the debug configurations.

2.如果想正常运行过程中通过调试器直接关闭看门狗，可以运行switch_jtag.bat,要求小机必须连接调试器，且jtag功能是开启的

If you want to directly close the watchdog through the debugger during normal operation, you can run the switch_jtag.bat. It is required that the target device must be connected to the debugger and the jtag function is enabled.

3.switch_adfu.bat是用于让小机自动进入adfu升级模式的脚本，包括uart的脚本和jtag执行脚本

The switch_adfu.bat is a script that allows the target device to automatically enter the adfu upgrade mode, including both uart and jtag execution scripts.