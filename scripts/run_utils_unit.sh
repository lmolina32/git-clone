#!/bin/bash

UNIT=unit_utils
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

TESTS_MAX=$(bin/$UNIT 2>&1 | tail -n 1 | awk '{print $1}' | tr -d '.')
TOTAL_COUNT=$((TESTS_MAX + 1))

for t in $(seq 0 $TESTS_MAX); do
    desc=$(bin/$UNIT 2>&1 | awk "/$t\./ { \$1=\$2=\"\"; print \$0 }")

    printf "%-60s... " "$desc"
    
    valgrind --leak-check=full --error-exitcode=1 bin/$UNIT $t &> $WORKSPACE/test
    VALGRIND_STATUS=$?
    
    LEAKS=$(awk '/ERROR SUMMARY:/ {print $4}' $WORKSPACE/test)

    if [ $VALGRIND_STATUS -ne 0 ] || [ "$LEAKS" -ne 0 ]; then
        error "Failure"
    else
        echo -e "${GREEN}Success${NC}"
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