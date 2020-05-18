@ECHO OFF

if "%1" == "x64_debug" (
  copy "%~dp0..\pthreads-w32\dll\x64\pthreadVC2.dll" "%~dp0target\x64\Debug\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\x64\Debug\librdkafkaC.dll" "%~dp0target\x64\Debug\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\x64\Debug\libzstd.dll" "%~dp0target\x64\Debug\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\x64\Debug\zlibd.dll" "%~dp0target\x64\Debug\"
)


if "%1" == "x64_release" (
  copy "%~dp0..\pthreads-w32\dll\x64\pthreadVC2.dll" "%~dp0target\x64\Release\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\x64\Release\librdkafkaC.dll" "%~dp0target\x64\Release\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\x64\Release\libzstd.dll" "%~dp0target\x64\Release\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\x64\Release\zlib.dll" "%~dp0target\x64\Release\"
)


if "%1" == "x86_debug" (
  copy "%~dp0..\pthreads-w32\dll\x86\pthreadVC2.dll" "%~dp0target\Win32\Debug\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\Win32\Debug\librdkafkaC.dll" "%~dp0target\Win32\Debug\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\Win32\Debug\libzstd.dll" "%~dp0target\Win32\Debug\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\Win32\Debug\zlibd.dll" "%~dp0target\Win32\Debug\"
)


if "%1" == "x86_release" (
  copy "%~dp0..\pthreads-w32\dll\x86\pthreadVC2.dll" "%~dp0target\Win32\Release\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\Win32\Release\librdkafkaC.dll" "%~dp0target\Win32\Release\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\Win32\Release\libzstd.dll" "%~dp0target\Win32\Release\"
  copy "%~dp0..\librdkafka-1.4.2\win32\outdir\v140\Win32\Release\zlib.dll" "%~dp0target\Win32\Release\"
)
