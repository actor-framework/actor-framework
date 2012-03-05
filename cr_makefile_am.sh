cpp_files=($(cat cppa.files | grep "src/.*\.cpp$" | sort))
num_cpp_files=${#cpp_files[@]}
hpp_files=($(cat cppa.files | grep "cppa/.*\.hpp$" | sort))
num_hpp_files=${#hpp_files[@]}

echo "ACLOCAL_AMFLAGS = -I m4"
echo ""
echo "lib_LTLIBRARIES = libcppa.la"
echo ""
echo "libcppa_la_SOURCES = \\"
for i in $(seq 0 $(($num_cpp_files - 2))); do echo "    ${cpp_files[$i]} \\" ; done
echo "    ${cpp_files[$(($num_cpp_files - 1))]}"
echo ""
echo "if VERSIONED_INCLUDE_DIR"
echo "library_includedir = \$(includedir)/cppa/\$(PACKAGE_VERSION)/"
echo "else"
echo "library_includedir = \$(includedir)/"
echo "endif"
echo ""
echo "nobase_library_include_HEADERS = \\"
for i in $(seq 0 $(($num_hpp_files - 2))); do echo "    ${hpp_files[$i]} \\" ; done
echo "    ${hpp_files[$(($num_hpp_files - 1))]}"
echo ""
echo "libcppa_la_CXXFLAGS = --std=c++0x -pedantic -Wall -Wextra"
echo "libcppa_la_LDFLAGS = -release \$(PACKAGE_VERSION) \$(BOOST_CPPFLAGS)"
echo ""
echo "pkgconfigdir = \$(libdir)/pkgconfig"
echo "pkgconfig_DATA = libcppa.pc"
echo ""
echo "SUBDIRS = . unit_testing examples benchmarks"
