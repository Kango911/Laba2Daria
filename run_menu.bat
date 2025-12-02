@echo off
chcp 65001 > nul
echo ============================================
echo ЗАПУСК ПРОГРАММЫ АЛГОРИТМА ХАФФМАНА
echo ============================================
echo.

if not exist huffman.exe (
    echo Ошибка: файл huffman.exe не найден!
    echo Сначала выполните compile.bat
    pause
    exit /b 1
)

echo Запуск программы с меню...
echo.
huffman.exe

pause