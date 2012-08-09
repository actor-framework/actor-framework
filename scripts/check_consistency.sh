command -v pdf2txt.py &>/dev/null 
if [ $? -ne 0 ]; then
    echo "pdf2txt.py not found"
    exit
fi

if [ ! -d manual ]; then
    echo "directory 'manual' does not exist"
    exit
fi

make -C manual &>/dev/null
PDF1=$(pdf2txt.py manual.pdf)
PDF2=$(pdf2txt.py manual/manual.pdf)
if [ "$PDF1" != "$PDF2" ] ; then
    echo "manual.pdf and manual/manual.pdf differ!"
    exit
fi

CMAKE_VERSION=$(grep -oE "set\(LIBCPPA_VERSION_(MAJOR|MINOR|PATCH) [0-9]+" CMakeLists.txt | awk '{ if (NR > 1) printf "." ; printf $2 } END { printf "\n" }')
MANUAL_VERSION=$(echo "$PDF1" | grep -oE "version [0-9]+(\.[0-9]+){2}" | awk '{print $2}')
CHANGELOG_VERSION=$(head -n1 ChangeLog | awk '{print $2}')

echo "libcppa version in CMakeLists.txt is $CMAKE_VERSION"
echo "libcppa version in manual.pdf is $MANUAL_VERSION"
echo "libcppa version in ChangeLog is $CHANGELOG_VERSION"

if [ "$CMAKE_VERSION" == "$MANUAL_VERSION" ] && [ "$CMAKE_VERSION" == "$CHANGELOG_VERSION" ]; then
    echo "no errors found"
else
    echo "versions differ"
fi
