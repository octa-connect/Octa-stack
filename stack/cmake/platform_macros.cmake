#This file contains helper MACRO's that are only available to platforms and chips

# Declare a parameter for the used platform. Platform parameters are 
# automaticall shown/hidden in the CMake GUI depending on whether or 
# not the platform is enabled.
# 
# Platform parameters are stored in the cache, just like regular parameters.
# This means that the properties of the parameter can be set in the normal manner
# (by using the SET_PROPERTY command)
#
# Usage:
#    PLATFORM_PARAM(<var> <default_value> <type> <doc_string>)
#	<var> 		is the name of the variable to set. By convention 
#			all platform parameters should be prefixed by the 
#			${PLATFORM_PREFIX} variable (see examples below).
#
#       <default_value> the default value of the parameter.
#
#	<type>		the type of the parameter. Any valid CMake cache <type>
#			except INTERNAL. At the time of writing these are the valid
#			types:
#				FILEPATH = File chooser dialog.
#				PATH     = Directory chooser dialog.
#				STRING   = Arbitrary string.
#				BOOL     = Boolean ON/OFF checkbox.
#				See http://www.cmake.org/cmake/help/v3.0/command/set.html
#
#	<doc_string>	the documentation string explaining the parameter
#
# Examples:
#	PLATFORM_PARAM(${PLATFORM_PREFIX}_STR "Default_Str" STRING "Parameter Explanation")
#		-Adds a STRING parameter '${PLATFORM_PREFIX}_STR' with default 
#		 value 'Default_Str' and explanation 'Parameter Explanation'
#		 The value of the parameter can be queried using:
#			${${PLATFORM_PREFIX}_STR}
#	
#	PLATFORM_PARAM(${PLATFORM_PREFIX}_LIST "item1" STRING "A List of items")
#	SET_PROPERTY(CACHE ${PLATFORM_PREFIX}_LIST PROPERTY STRINGS "item1;item2;item3")
#		-Adds a parameter '${PLATFORM_PREFIX}_LIST', with possible values
#		 'item1', 'item2' and 'item3'. Please note that this is done in
#		 exactly the same manner as for regular 'CACHE' parameters
#
MACRO(PLATFORM_PARAM var value type doc)
    SET(PLATFORM_PARAM_LIST "${PLATFORM_PARAM_LIST};${var}" CACHE INTERNAL "")
    #Set entry in cache if this the platform us being used is being built
    SETCACHE_IF(${var} ${value} ${type} ${doc} ${PLATFORM_PREFIX})
ENDMACRO()

# Declare an options for the used platform. Like platform parameters,
# platform options are automatically shown/hidden in the CMake GUI 
# depending on whether or not the platform is enabled.
#
# As with platform parameters, options are stored in the cache (just like
# regular CMake options)
#
# Usage:
#    PLATFORM_OPTION(<option> <doc_string> <default_value>)
#	<option>	is the name of the option to set. By convention 
#			all platform options should be prefixed by the 
#			${PLATFORM_PREFIX} variable.
#
#	<doc_string>	the documentation string explaining the option
#
#       <default_value> the default value of the parameter.
#
#    In accordance with CMake's implementation of 'ADD_OPTION',
#    calling: 
#	PLATFORM_OPTION(<option> <doc_string> <default_value>)
#    is equivalent to calling:
#	PLATFORM_PARAM( <option> <default_value> BOOL <doc_string>
#
# Examples:
#	PLATFORM_OPTION(${PLATFORM_PREFIX}_OPT "OPT Explanation" FALSE)
#		-Adds a platform option '{PLATFORM_PREFIX}_OPT' with 
#		 default value FALSE and explanation 'OPT Explanation'.
#		 The value of the option can be queried using:
#		    ${${PLATFORM_PREFIX}_OPT}
#
MACRO(PLATFORM_OPTION option doc default)
    PLATFORM_PARAM( ${option} ${default} BOOL ${doc})
ENDMACRO()

# Include drivers for a chip <chip> into the current platform. 
# Code for individual chips are stored in the 'framework/hal/chips' 
# directory where each chip is a separate subdirectory.

# Each chip directory must contain a 'CMakeLists.txt' file that defines 
# an object library with name '${CHIP_LIBRARY_NAME}'.
##
# Usage:
#    ADD_CHIP(<chip>)
#	<chip>		The chip directory to include
#
MACRO(ADD_CHIP chip)
    GEN_PREFIX(CHIP_LIBRARY_NAME "CHIP" ${chip})
    ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/../../chips/${chip}" "${CMAKE_CURRENT_BINARY_DIR}/../../chips/${chip}")
    SET_GLOBAL(PLATFORM_CHIP_LIBS "${PLATFORM_CHIP_LIBS};${CHIP_LIBRARY_NAME}")   
    UNSET(CHIP_LIBRARY_NAME)
ENDMACRO()

# Add directories to the 'INCLUDE_DIRECTORIES' property of the directory 
# containing the platform specific code. This MACRO is intended to be used 
# in the CMakeLists.txt file of individual chips and allows each indiviual chip to 
# export interfaces to the platform code (but NOT the entire source tree). 
#
# This is, for instance, usefull if platform specific code needs access to 'private' 
# chip-specific headers.
#
# Usage:
#    PLATFORM_INCLUDE_DIRECTORIES(<dir> <dir> ...)
#
MACRO(PLATFORM_INCLUDE_DIRECTORIES)
    INCLUDE_DIRECTORIES(${ARGN})
    SET(__platform_dir "${CMAKE_SOURCE_DIR}/framework/hal/platforms/${PLATFORM}")
    GET_PROPERTY(__platform_props DIRECTORY "${__platform_dir}" PROPERTY INCLUDE_DIRECTORIES)
    SET_PROPERTY(DIRECTORY "${__platform_dir}" PROPERTY INCLUDE_DIRECTORIES ${__platform_props} ${ARGN})
    UNSET(__platform_dir)
    UNSET(__platform_props)
ENDMACRO()
