#!/bin/bash

TEST_BIN="$1"
UNIT=$(basename "$TEST_BIN")
WORKSPACE=/tmp/$UNIT.$(id -u)
FAILURES=0

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

error() {
    echo -e "${RED}$@${NC}"
    [ -r $WORKSPACE/test ] && (echo; cat $WORKSPACE/test; echo)
    FAILURES=$((FAILURES + 1))
}

cleanup() {
    STATUS=${1:-$FAILURES}
    rm -fr $WORKSPACE
    exit $STATUS
}

mkdir -p $WORKSPACE
trap "cleanup" EXIT
trap "cleanup 1" INT TERM

echo -e "\nTesting ${UNIT} ..."

if [ ! -x bin/$UNIT ]; then
    error "Failure: bin/$UNIT is not executable!"
    exit 1
fi

USE_SANITIZER=0
USE_VALGRIND=0
USE_LEAKS=0

if [ "${SANITIZE:-0}" = "1" ]; then
    USE_SANITIZER=1
    echo -e "Tool: ${GREEN}AddressSanitizer/UndefinedBehaviorSanitizer${NC} (built-in)"
elif command -v valgrind &> /dev/null; then
    USE_VALGRIND=1
    echo -e "Tool: ${GREEN}Valgrind${NC} detected."
elif [ "$(uname)" = "Darwin" ] && command -v leaks &> /dev/null; then
    USE_LEAKS=1
    echo -e "Tool: macOS ${GREEN}leaks${NC}."
else
    echo -e "Tool: ${RED}None${NC} (no memory checker available)"
fi

TESTS_MAX=$(bin/$UNIT 2>&1 | tail -n 1 | awk '{print $1}' | tr -d '.')
TOTAL_COUNT=$((TESTS_MAX + 1))

for t in $(seq 0 $TESTS_MAX); do
    desc=$(bin/$UNIT 2>&1 | awk "/$t\./ { \$1=\$2=\"\"; print \$0 }")

    printf "%-60s... " "$desc"

    if [ $USE_SANITIZER -eq 1 ]; then
        "$TEST_BIN" "$t" &> "$WORKSPACE/test"
        STATUS=$?
        if [ $STATUS -ne 0 ]; then
            error "Failure (exit code $STATUS)"
        else
            if grep -E "ERROR: (Address|Leak|Undefined)Sanitizer" "$WORKSPACE/test" > /dev/null; then
                error "Failure (sanitizer report)"
            else
                echo -e "${GREEN}Success${NC}"
            fi
        fi
    elif [ $USE_VALGRIND -eq 1 ]; then
        valgrind --leak-check=full \
                 --show-leak-kinds=all \
                 --track-origins=yes \
                 --error-exitcode=1 \
                 "$TEST_BIN" "$t" &> "$WORKSPACE/test"
        STATUS=$?
        LEAKS=$(awk '/ERROR SUMMARY:/ {print $4}' "$WORKSPACE/test")
        if [ $STATUS -ne 0 ] || [ "$LEAKS" -ne 0 ]; then
            error "Failure"
        else
            echo -e "${GREEN}Success${NC}"
        fi
    elif [ $USE_LEAKS -eq 1 ]; then
        env MallocStackLogging=1 leaks -atExit -- "$TEST_BIN" "$t" &> "$WORKSPACE/test"
        STATUS=$?
        if [ $STATUS -ne 0 ]; then
            error "Failure"
        else
            if grep -q "LEAK:" "$WORKSPACE/test"; then
                error "Failure (leaks detected)"
            else
                echo -e "${GREEN}Success${NC}"
            fi
        fi
    else
        "$TEST_BIN" "$t" &> "$WORKSPACE/test"
        if [ $? -ne 0 ]; then
            error "Failure"
        else
            echo -e "${GREEN}Success${NC}"
        fi
    fi
done

echo "------------------------------------------------------------"
if [ $FAILURES -eq 0 ]; then
    echo -e "${GREEN}PASS: $TOTAL_COUNT/$TOTAL_COUNT tests passed.${NC}"
else
    PASSED=$((TOTAL_COUNT - FAILURES))
    echo -e "${RED}FAIL: $FAILURES/$TOTAL_COUNT tests failed (Passed: $PASSED).${NC}"
fi
echo "------------------------------------------------------------"