@echo off
chcp 65001 > nul
echo ============================================
echo КОМПИЛЯЦИЯ ПРОГРАММЫ АЛГОРИТМА ХАФФМАНА
echo ============================================
echo.

echo Компиляция main.c в huffman.exe...
gcc main.c -o huffman.exe -Wall -Wextra

if %errorlevel% equ 0 (
    echo Успешно скомпилировано!
    echo Исполняемый файл: huffman.exe
) else (
    echo Ошибка компиляции!
    pause
    exit /b 1
)

echo.
echo ============================================
echo Для запуска программы используйте:
echo   1. huffman.exe                     - меню
echo   2. huffman.exe input.txt out.bin decoded.txt
echo ============================================
pause