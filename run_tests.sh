#!/bin/bash

# Скрипт для запуска тестов проекта

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

check_build() {
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Директория build не найдена!"
        print_info "Запустите: cmake -B build && cmake --build build"
        exit 1
    fi
}

build_project() {
    print_header "Сборка проекта"
    cmake --build "$BUILD_DIR" --config Release
    if [ $? -eq 0 ]; then
        print_success "Сборка завершена успешно"
    else
        print_error "Ошибка сборки"
        exit 1
    fi
}

run_all_tests() {
    print_header "Запуск всех тестов"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
    return $?
}

run_test() {
    local test_name=$1
    local verbose=$2
    
    print_header "Запуск теста: $test_name"
    
    if [ "$verbose" = "true" ]; then
        ctest --test-dir "$BUILD_DIR" -R "$test_name" -V
    else
        ctest --test-dir "$BUILD_DIR" -R "$test_name" --output-on-failure
    fi
    
    return $?
}

list_tests() {
    print_header "Доступные тесты"
    echo "1. PeerConnectionTest - базовый тест peer-connection"
    echo "2. PeerConnectionRecorderTest - тест AudioRecorder → PeerConnection"
    echo "3. PeerConnectionReceiverTest - тест PeerConnection → AudioReceiver"
    echo "4. PeerConnectionFullTest - полный интеграционный тест"
    echo "5. Все тесты"
    echo "6. Выход"
}

show_menu() {
    while true; do
        echo ""
        list_tests
        echo ""
        read -p "Выберите тест (1-6): " choice
        
        case $choice in
            1)
                run_test "PeerConnectionTest" "false"
                ;;
            2)
                run_test "PeerConnectionRecorderTest" "false"
                ;;
            3)
                run_test "PeerConnectionReceiverTest" "false"
                ;;
            4)
                run_test "PeerConnectionFullTest" "false"
                ;;
            5)
                run_all_tests
                ;;
            6)
                print_info "Выход"
                exit 0
                ;;
            *)
                print_error "Неверный выбор. Попробуйте снова."
                ;;
        esac
        
        if [ $? -eq 0 ]; then
            print_success "Тест завершен успешно"
        else
            print_error "Тест завершился с ошибкой"
        fi
    done
}

# Обработка аргументов командной строки
if [ $# -eq 0 ]; then
    check_build
    show_menu
elif [ "$1" = "--all" ] || [ "$1" = "-a" ]; then
    check_build
    run_all_tests
elif [ "$1" = "--build" ] || [ "$1" = "-b" ]; then
    check_build
    build_project
    run_all_tests
elif [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "Использование: $0 [опции]"
    echo ""
    echo "Опции:"
    echo "  (без опций)     - интерактивное меню"
    echo "  --all, -a       - запустить все тесты"
    echo "  --build, -b     - собрать проект и запустить все тесты"
    echo "  --help, -h      - показать эту справку"
    echo ""
    echo "Примеры:"
    echo "  $0                    # Интерактивное меню"
    echo "  $0 --all              # Запустить все тесты"
    echo "  $0 --build            # Собрать и запустить все тесты"
elif [ "$1" = "--test" ] || [ "$1" = "-t" ]; then
    if [ -z "$2" ]; then
        print_error "Укажите имя теста"
        exit 1
    fi
    check_build
    run_test "$2" "false"
else
    print_error "Неизвестная опция: $1"
    echo "Используйте --help для справки"
    exit 1
fi

