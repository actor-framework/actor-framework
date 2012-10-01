command -v pdf2txt.py &>/dev/null 
if [ $? -ne 0 ]; then
    echo "pdf2txt.py not found"
    exit
fi

if [ ! -d manual ]; then
    echo "directory 'manual' does not exist"
    exit
fi

echo "build manual/manual.pdf"
make -C manual pdf &>/dev/null
if [ $? -ne 0 ]; then
    echo "failure during build!"
    exit
fi
echo "read manual.pdf"
PDF1=$(pdf2txt.py manual.pdf)
echo "read manual/manual.pdf"
PDF2=$(pdf2txt.py manual/manual.pdf)
if [ "$PDF1" != "$PDF2" ] ; then
    echo "manual.pdf and manual/manual.pdf differ!"
    exit
else
    echo "manual.pdf up-to-date"
fi

function expand_version_string {
    echo "$1.0" | awk 'BEGIN {FS="."};{printf $1 "." $2 "." $3}';
}

CMAKE_VERSION=$(grep -oE "set\(LIBCPPA_VERSION_(MAJOR|MINOR|PATCH) [0-9]+" CMakeLists.txt | awk '{ if (NR > 1) printf "." ; printf $2 } END { printf "\n" }')
MANUAL_VERSION=$(echo "$PDF1" | grep -oE "version [0-9]+(\.[0-9]+){1,2}" | awk '{print $2}')
CHANGELOG_VERSION=$(head -n1 ChangeLog.md | awk '{print $2}')
DOCU_VERSION=$(grep -oE "Version [0-9]+(\.[0-9]+){1,2}" Doxyfile.in  | awk '{print $2}')

MANUAL_VERSION=$(expand_version_string "$MANUAL_VERSION")
CHANGELOG_VERSION=$(expand_version_string "$CHANGELOG_VERSION")
DOCU_VERSION=$(expand_version_string "$DOCU_VERSION")

echo "libcppa version in CMakeLists.txt is $CMAKE_VERSION"
echo "libcppa version in manual.pdf is $MANUAL_VERSION"
echo "libcppa version in ChangeLog is $CHANGELOG_VERSION"
echo "libcppa version in documentation is $DOCU_VERSION"

if [ "$CMAKE_VERSION" == "$MANUAL_VERSION" ] && [ "$CMAKE_VERSION" == "$CHANGELOG_VERSION" ] && [ "$CMAKE_VERSION" == "$DOCU_VERSION" ]; then
    echo "no errors found"
else
    echo "versions differ"
fi
