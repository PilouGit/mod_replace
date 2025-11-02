cmake_minimum_required(VERSION 3.10)
project(mod_replace C)

set(CMAKE_C_STANDARD 99)

# Find Apache development files
find_program(APXS_EXECUTABLE NAMES apxs2 apxs
    PATHS /usr/bin /usr/sbin /usr/local/bin /usr/local/sbin
    DOC "Apache apxs tool")

if(NOT APXS_EXECUTABLE)
    message(FATAL_ERROR "apxs tool not found. Please install apache2-dev or httpd-devel package")
endif()

# Get Apache configuration
execute_process(
    COMMAND ${APXS_EXECUTABLE} -q CFLAGS
    OUTPUT_VARIABLE APACHE_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${APXS_EXECUTABLE} -q INCLUDEDIR
    OUTPUT_VARIABLE APACHE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${APXS_EXECUTABLE} -q LIBEXECDIR
    OUTPUT_VARIABLE APACHE_MODULE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${APXS_EXECUTABLE} -q SYSCONFDIR
    OUTPUT_VARIABLE APACHE_CONF_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Find APR
find_program(APR_CONFIG_EXECUTABLE NAMES apr-1-config apr-config
    PATHS /usr/bin /usr/local/bin
    DOC "APR configuration tool")

if(APR_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${APR_CONFIG_EXECUTABLE} --cflags
        OUTPUT_VARIABLE APR_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    execute_process(
        COMMAND ${APR_CONFIG_EXECUTABLE} --includes
        OUTPUT_VARIABLE APR_INCLUDES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

# Module source
set(MODULE_NAME mod_replace)
set(MODULE_SOURCE src/mod_replace.c)

# Aho-Corasick library
set(AHO_CORASICK_SOURCES src/aho_corasick.c)
add_library(aho_corasick STATIC ${AHO_CORASICK_SOURCES})
set_property(TARGET aho_corasick PROPERTY POSITION_INDEPENDENT_CODE ON)

# Include directories for aho_corasick
target_include_directories(aho_corasick PUBLIC inc)

# Create shared library
add_library(${MODULE_NAME} SHARED ${MODULE_SOURCE})

# Link aho_corasick to the module
target_link_libraries(${MODULE_NAME} PRIVATE aho_corasick)

# Set library properties
set_target_properties(${MODULE_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME ${MODULE_NAME}
)

# Include directories
target_include_directories(${MODULE_NAME} PRIVATE 
    ${APACHE_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/inc
)

# Compile flags
if(APACHE_CFLAGS)
    separate_arguments(APACHE_CFLAGS_LIST UNIX_COMMAND "${APACHE_CFLAGS}")
    target_compile_options(${MODULE_NAME} PRIVATE ${APACHE_CFLAGS_LIST})
endif()

if(APR_CFLAGS)
    separate_arguments(APR_CFLAGS_LIST UNIX_COMMAND "${APR_CFLAGS}")
    target_compile_options(${MODULE_NAME} PRIVATE ${APR_CFLAGS_LIST})
endif()

if(APR_INCLUDES)
    separate_arguments(APR_INCLUDES_LIST UNIX_COMMAND "${APR_INCLUDES}")
    target_compile_options(${MODULE_NAME} PRIVATE ${APR_INCLUDES_LIST})
endif()

# Additional compile flags for Apache modules
target_compile_definitions(${MODULE_NAME} PRIVATE
    -DLINUX
    -D_REENTRANT
    -D_GNU_SOURCE
)

# Position Independent Code for shared library
set_property(TARGET ${MODULE_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Generate mod_replace.load file
configure_file(
    ${CMAKE_SOURCE_DIR}/mod_replace.load.in
    ${CMAKE_BINARY_DIR}/mod_replace.load
    @ONLY
)

# Install target
install(TARGETS ${MODULE_NAME}
    LIBRARY DESTINATION ${APACHE_MODULE_DIR}
)

install(FILES ${CMAKE_BINARY_DIR}/mod_replace.load
    DESTINATION ${APACHE_CONF_DIR}/mods-available
)

# Custom targets for Apache operations
add_custom_target(install-module
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install
    COMMAND a2enmod replace || echo "Module enabled manually"
    DEPENDS ${MODULE_NAME}
    COMMENT "Installing and enabling Apache module"
)

add_custom_target(uninstall-module
    COMMAND a2dismod replace || echo "Module disabled manually"
    COMMAND ${CMAKE_COMMAND} -E remove ${APACHE_MODULE_DIR}/${MODULE_NAME}.so
    COMMAND ${CMAKE_COMMAND} -E remove ${APACHE_CONF_DIR}/mods-available/mod_replace.load
    COMMENT "Disabling and uninstalling Apache module"
)

add_custom_target(reload-apache
    COMMAND systemctl reload apache2 || service apache2 reload || /etc/init.d/apache2 reload
    COMMENT "Reloading Apache configuration"
)

add_custom_target(restart-apache
    COMMAND systemctl restart apache2 || service apache2 restart || /etc/init.d/apache2 restart
    COMMENT "Restarting Apache service"
)

# Combined target for easy deployment
add_custom_target(deploy
    DEPENDS install-module reload-apache
    COMMENT "Installing module and reloading Apache"
)

# Enable testing
enable_testing()

# Test executable
set(TEST_NAME test_valgrind)
add_executable(${TEST_NAME} test/test_standalone.c)

# Link aho_corasick library to test
target_link_libraries(${TEST_NAME} PRIVATE aho_corasick)

# Include directories for test
target_include_directories(${TEST_NAME} PRIVATE 
    ${APACHE_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/inc
)

# Compile flags for test
if(APACHE_CFLAGS)
    target_compile_options(${TEST_NAME} PRIVATE ${APACHE_CFLAGS_LIST})
endif()

if(APR_CFLAGS)
    target_compile_options(${TEST_NAME} PRIVATE ${APR_CFLAGS_LIST})
endif()

if(APR_INCLUDES)
    target_compile_options(${TEST_NAME} PRIVATE ${APR_INCLUDES_LIST})
endif()

# Compile definitions for test
target_compile_definitions(${TEST_NAME} PRIVATE
    -DLINUX
    -D_REENTRANT
    -D_GNU_SOURCE
    -DTEST_BUILD
)

# Find and link APR libraries for test
find_library(APR_LIBRARY NAMES apr-1 apr)
find_library(APRUTIL_LIBRARY NAMES aprutil-1 aprutil)

if(APR_LIBRARY AND APRUTIL_LIBRARY)
    target_link_libraries(${TEST_NAME} PRIVATE ${APR_LIBRARY} ${APRUTIL_LIBRARY})
else()
    # Fallback to pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(APR apr-1)
        pkg_check_modules(APRUTIL apr-util-1)
        if(APR_FOUND AND APRUTIL_FOUND)
            target_link_libraries(${TEST_NAME} PRIVATE ${APR_LIBRARIES} ${APRUTIL_LIBRARIES})
            target_link_directories(${TEST_NAME} PRIVATE ${APR_LIBRARY_DIRS} ${APRUTIL_LIBRARY_DIRS})
        endif()
    endif()
endif()

# Add test to CTest
add_test(NAME valgrind_test COMMAND ${TEST_NAME})

# Aho-Corasick test executable
set(AC_TEST_NAME test_aho_corasick)
add_executable(${AC_TEST_NAME} test/test_aho_corasick.c)
target_link_libraries(${AC_TEST_NAME} PRIVATE aho_corasick)
target_include_directories(${AC_TEST_NAME} PRIVATE inc)

# Add aho-corasick test to CTest
add_test(NAME aho_corasick_test COMMAND ${AC_TEST_NAME})

# Custom target for valgrind testing
add_custom_target(valgrind-test
    COMMAND valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-test.log $<TARGET_FILE:${TEST_NAME}>
    DEPENDS ${TEST_NAME}
    COMMENT "Running valgrind memory check on test"
)

# Custom target for aho-corasick valgrind testing
add_custom_target(valgrind-aho-corasick
    COMMAND valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-aho-corasick.log $<TARGET_FILE:${AC_TEST_NAME}>
    DEPENDS ${AC_TEST_NAME}
    COMMENT "Running valgrind memory check on Aho-Corasick test"
)

# Print configuration summary
message(STATUS "Apache Module Configuration:")
message(STATUS "  APXS tool: ${APXS_EXECUTABLE}")
message(STATUS "  Include directory: ${APACHE_INCLUDE_DIR}")
message(STATUS "  Module directory: ${APACHE_MODULE_DIR}")
message(STATUS "  Config directory: ${APACHE_CONF_DIR}")
if(APR_CONFIG_EXECUTABLE)
    message(STATUS "  APR config: ${APR_CONFIG_EXECUTABLE}")
endif()