@echo off
setlocal EnableDelayedExpansion

echo 〉〉〉自动清理〈〈〈
for /r %%F in (*.d *.map *.gch) do (
    if exist "%%F" (
        echo 删除：%%F
        del /q "%%F"
    )
)

echo.
where upx >nul 2>&1
if errorlevel 1 (
    echo [警告] 未找到 upx，请确认 upx.exe 在 PATH 中
    goto :end
)

for /r %%E in (*.exe) do (
    echo 压缩：%%E
    upx --best "%%E" >nul 2>&1
)

pause