@echo off
set IDF_PATH=C:\esp\v5.4.4\esp-idf
set IDF_TOOLS_PATH=C:\Espressif\tools
set IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.4.4\venv
set MSYSTEM=
set PATH=C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\ccache\4.10.2\ccache-4.10.2-windows-x86_64;C:\Espressif\tools\python\v5.4.4\venv\Scripts;C:\Espressif\tools\esp-clang\19.1.2\esp-clang\bin;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Espressif\tools\xtensa-esp32s3-elf\esp-14.2.0_20260121\xtensa-esp32s3-elf\bin;%PATH%
cd /d D:\work\desktop_girlfriend
C:\Espressif\tools\python\v5.4.4\venv\Scripts\python.exe %IDF_PATH%\tools\idf.py %*
