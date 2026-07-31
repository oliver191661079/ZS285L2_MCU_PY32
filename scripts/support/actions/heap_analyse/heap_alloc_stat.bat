REM
REM 该批处理用于解析串口日志系统堆内存使用状况
REM 脚本解析日志后，会首先分析所有的malloc和free打印，确认malloc和free是否成对出现，成对出现的认为是已经释放的动态内存，否则就是未释放的内存
REM 统计完成后，会生成两个log文件，分别列出当前已经释放的动态内存和未释放的动态内存，这样就可以看出系统是否存在频繁申请释放的大的动态内存
REM 同时会根据caller信息解析elf文件得到函数调用关系
REM 如果串口日志有sysheap dump的信息，会对heap信息也做解析，解析当前动态内存申请的具体状况，查看系统碎片情况，以及page的分配情况

python heap_alloc_stat.py -i heap_alloc_1_单箱连接.log -e ..\..\..\..\samples\bt_speaker\outdir\ats2875h_evb_charge6\zephyr.elf
python heap_alloc_stat.py -i heap_alloc_2_单箱蓝牙推歌.log -e ..\..\..\..\samples\bt_speaker\outdir\ats2875h_evb_charge6\zephyr.elf
python heap_alloc_stat.py -i heap_alloc_3_单箱蓝牙推歌+app.log -e ..\..\..\..\samples\bt_speaker\outdir\ats2875h_evb_charge6\zephyr.elf
python heap_alloc_stat.py -i heap_alloc_4_单箱蓝牙推歌+OTA.log -e ..\..\..\..\samples\bt_speaker\outdir\ats2875h_evb_charge6\zephyr.elf
python heap_alloc_stat.py -i heap_alloc_5_单箱蓝牙推歌+OTA取消升级.log -e ..\..\..\..\samples\bt_speaker\outdir\ats2875h_evb_charge6\zephyr.elf
python heap_alloc_stat.py -i heap_alloc_6_BIS主箱蓝牙推歌+app.log -e ..\..\..\..\samples\bt_speaker\outdir\ats2875h_evb_charge6\zephyr.elf
pause