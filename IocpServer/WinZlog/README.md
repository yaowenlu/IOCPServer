WinZlog
=================

##Zlog on Windows

��zlog Linux��https://github.com/HardySimpson/zlog

��Windows�ϵ���ֲhttps://github.com/pattheaux/zlog

�Ļ�����ʹ��VS2010�������������̡�

Release�ļ�������VS2010�������ɵ�.lib��.dll��zlogͷ�ļ����Ը���zlog�ĵ�ֱ����ʹ�á�

���빤�����������⣺

Requires the glob function from the Unixem project: 
http://synesis.com.au/software/unixem.html 

Requires the pthreads-win32 library: 
http://www.sourceware.org/pthreads-win32/

Project tree

    ����extlib  Requires libs
    ��  ����Pre-built.2
    ��  ��  ����dll
    ��  ��  ��  ����x64
    ��  ��  ��  ����x86
    ��  ��  ����include
    ��  ��  ����lib
    ��  ��      ����x64
    ��  ��      ����x86
    ��  ����unixem-1.9.1
    ��      ����build
    ��      ����doc
    ��      ��  ����html
    ��      ��      ����1.9.1
    ��      ����include
    ��      ����include_error
    ��      ����lib
    ��      ����src
    ��      ��  ����internal
    ��      ����test
    ����Release  VS2010 output .lib .dll and zlog include file
    ��  ����bin
    ��  ����demo
    ��  ����head
    ��  ����lib
    ����test   
    ����zlog   VS2010 Project
    ����bin
    ����doc
    ����lib
    ����Release
    ����src
    ����test
    ����tools