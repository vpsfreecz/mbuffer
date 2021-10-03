#
#  Copyright (C) 2019, Thomas Maier-Komor
#
#  This is source code of mbuffer.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.1.0)

include(CheckStructHasMember)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CMakePrintHelpers)

project(mbuffer C)

find_package(Threads)

set(PACKAGE_VERSION "20191016")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_LD_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_LD_FLAGS_RELEASE "-O2")
set(TESTTREE "..")
add_compile_options("-Wall")
add_definitions(-DDEBUG)

find_program(AWK NAMES gawk awk)
find_program(MT mt)
find_program(OPENSSL openssl)
find_program(NM NAMES gnm nm)
find_program(SH NAMES bash sh)
find_program(DIFF NAMES gdiff diff)
find_program(TAR NAMES gtar tar)


set(TESTFILE "test.tar" CACHE PATH "test input file")
#message(STATUS "testfile is ${TESTFILE}")

check_struct_has_member("struct stat" st_blksize sys/stat.h HAVE_STRUCT_STAT_ST_BLKSIZE LANGUAGE C)
check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME)
check_library_exists(crypto MD5_Init "openssl/md5.h" HAVE_LIBCRYPTO)

if (HAVE_LIBCRYPTO)
	set(LIBS ${LIBS} crypto)
endif()

check_include_file(alloca.h HAVE_ALLOCA_H)
check_include_file(md5.h HAVE_MD5_H)
check_include_file(sys/sendfile.h HAVE_SENDFILE_H)

check_symbol_exists(alloca stdlib.h;alloca.h HAVE_ALLOCA)
check_symbol_exists(sendfile "sendfile.h;sys/sendfile.h;sys/socket.h" HAVE_SENDFILE)
check_symbol_exists(hstrerror netdb.h HAVE_HSTRERROR)
check_symbol_exists(getaddrinfo "netdb.h;sys/socket.h" HAVE_GETADDRINFO)
check_symbol_exists(mmap sys/mman.h HAVE_MMAP)
check_symbol_exists(seteuid unistd.h HAVE_SETEUID)


find_library(LIBSENDFILE sendfile)
if (LIBSENDFILE)
	set(LIBS ${LIBS} ${LIBSENDFILE})
endif()

find_library(LIBDL dl)
if (LIBDL)
	set(LIBS ${LIBS} ${LIBDL})
endif()

find_library(LIBRT rt)
if (LIBRT)
	set(LIBS ${LIBS} ${LIBRT})
endif()

find_library(LIBRESOLV resolv)
if (LIBRESOLV)
	set(LIBS ${LIBS} ${LIBRESOLV})
endif()

find_library(LIBSOCKET socket)
if (LIBSOCKET)
	set(LIBS ${LIBS} ${LIBSOCKET})
endif()

find_library(LIBNSL nsl)
if (LIBNSL)
	set(LIBS ${LIBS} ${LIBNSL})
endif()

find_library(LIBM m)
if (LIBM)
	set(LIBS ${LIBS} ${LIBM})
endif()

add_executable(mbuffer
	mbuffer.c
	input.c
	network.c
	hashing.c
	common.c
	globals.c
	settings.c
	log.c
)

target_link_libraries(
	mbuffer
	${LIBS}
	${CMAKE_THREAD_LIBS_INIT}
)

target_compile_definitions(
	mbuffer
	PRIVATE PREFIX="${CMAKE_INSTALL_PREFIX}"
)

install(FILES
	mbuffer
	DESTINATION bin
)

# testing section

if (ENABLE_TESTING)
	enable_testing()

	try_compile(LFNAMES ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/lf_names.c COPY_FILE ${PROJECT_BINARY_DIR}/lf_names)
	if (LFNAMES)
		execute_process(
			COMMAND ${NM} -u -P ${PROJECT_BINARY_DIR}/lf_names
			OUTPUT_FILE "${PROJECT_BINARY_DIR}/lf_names.txt"
		)
		execute_process(
			COMMAND ${AWK} "/open/ { sub(\"@.*\",\"\",$1); printf(\"%s\",\$1); }" ${PROJECT_BINARY_DIR}/lf_names.txt
			OUTPUT_VARIABLE LF_OPEN
		)
		execute_process(
			COMMAND ${AWK} "/fstat/ { sub(\"@.*\",\"\",$1); printf(\"%s\",\$1); }" ${PROJECT_BINARY_DIR}/lf_names.txt
			OUTPUT_VARIABLE LF_FSTAT
		)
		execute_process(
			COMMAND ${AWK} "/read/ { sub(\"@.*\",\"\",$1); printf(\"%s\",\$1); }" ${PROJECT_BINARY_DIR}/lf_names.txt
			OUTPUT_VARIABLE LF_READ
		)
		execute_process(
			COMMAND ${AWK} "/write/ { sub(\"@.*\",\"\",$1); printf(\"%s\",\$1); }" ${PROJECT_BINARY_DIR}/lf_names.txt
			OUTPUT_VARIABLE LF_WRITE
		)
		cmake_print_variables(LF_OPEN)
	endif()

	add_executable(have-af have-af.c)

	add_library(idev SHARED idev.c)
	add_library(tapetest SHARED tapetest.c)

	set_target_properties(idev tapetest
		PROPERTIES
		INTERFACE_POSITION_INDEPENDENT_CODE ON
	)
	
	add_test(NAME test0 COMMAND ${SH} -x test0.sh ${TESTFILE})
	add_test(NAME test1 COMMAND ${SH} -x test1.sh ${TESTFILE})
	add_test(NAME test2 COMMAND ${SH} -x test2.sh ${TESTFILE})
	add_test(NAME test3 COMMAND ${SH} -x test3.sh ${TESTFILE})
	add_test(NAME test4 COMMAND ${SH} -x test4.sh ${TESTFILE})
	add_test(NAME test5 COMMAND ${SH} -x test5.sh ${TESTFILE})
	add_test(NAME test6 COMMAND ${SH} -x test6.sh ${TESTFILE})
	add_test(NAME test7 COMMAND ${SH} -x test7.sh ${TESTFILE})
	
	set_tests_properties(test0 test1 test2 test3 test4 test5 test6 test7
		PROPERTIES
		ENVIRONMENT "DIFF=${DIFF};OPENSSL=${OPENSSL}"
		WORKING_DIRECTORY tests
		DEPENDS test.tar
	)
	
	set_tests_properties(test4 PROPERTIES DEPENDS tapetest)
	set_tests_properties(test5 PROPERTIES DEPENDS tapetest)
	#set_tests_properties(test6 PROPERTIES DEPENDS idev)
	
endif()

configure_file(config.h.cm config.h)

