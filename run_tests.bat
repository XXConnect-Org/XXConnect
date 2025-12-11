@echo off
REM Скрипт для запуска тестов проекта (Windows)

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build

if not exist "%BUILD_DIR%" (
    echo Ошибка: Директория build не найдена!
    echo Запустите: cmake -B build ^&^& cmake --build build
    exit /b 1
)

if "%1"=="" (
    echo.
    echo ========================================
    echo Доступные тесты:
    echo ========================================
    echo 1. PeerConnectionTest - базовый тест peer-connection
    echo 2. PeerConnectionRecorderTest - тест AudioRecorder -^> PeerConnection
    echo 3. PeerConnectionReceiverTest - тест PeerConnection -^> AudioReceiver
    echo 4. PeerConnectionFullTest - полный интеграционный тест
    echo 5. Все тесты
    echo.
    set /p choice="Выберите тест (1-5): "
    
    if "!choice!"=="1" (
        ctest --test-dir "%BUILD_DIR%" -R PeerConnectionTest --output-on-failure
    ) else if "!choice!"=="2" (
        ctest --test-dir "%BUILD_DIR%" -R PeerConnectionRecorderTest --output-on-failure
    ) else if "!choice!"=="3" (
        ctest --test-dir "%BUILD_DIR%" -R PeerConnectionReceiverTest --output-on-failure
    ) else if "!choice!"=="4" (
        ctest --test-dir "%BUILD_DIR%" -R PeerConnectionFullTest --output-on-failure
    ) else if "!choice!"=="5" (
        ctest --test-dir "%BUILD_DIR%" --output-on-failure
    ) else (
        echo Неверный выбор
        exit /b 1
    )
) else if "%1"=="--all" (
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
) else if "%1"=="--build" (
    cmake --build "%BUILD_DIR%" --config Release
    if errorlevel 1 exit /b 1
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
) else if "%1"=="--help" (
    echo Использование: %0 [опции]
    echo.
    echo Опции:
    echo   (без опций)     - интерактивное меню
    echo   --all           - запустить все тесты
    echo   --build         - собрать проект и запустить все тесты
    echo   --help          - показать эту справку
) else (
    echo Неизвестная опция: %1
    echo Используйте --help для справки
    exit /b 1
)

