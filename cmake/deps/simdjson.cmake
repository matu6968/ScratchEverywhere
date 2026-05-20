function(_dep_system_simdjson)
	find_package(simdjson QUIET)
endfunction()

function(_dep_source_simdjson)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME simdjson
		GITHUB_REPOSITORY simdjson/simdjson
		VERSION 4.6.4
	)
endfunction()
