
function(add_extension TARGET)
	add_library(${TARGET} SHARED ${ARGN})
	set_target_properties(${TARGET} PROPERTIES PREFIX "")
	target_link_libraries(${TARGET} PRIVATE se-interface)

	# Shared libraries on Android can't have undefined symbols, so the extension has to be linked to the runtime
	if(ANDROID)
		target_link_libraries(${TARGET} PRIVATE scratch-everywhere)
	endif()

	if(APPLE)
		target_link_options(${TARGET} PRIVATE -undefined dynamic_lookup)
	endif()
endfunction()
