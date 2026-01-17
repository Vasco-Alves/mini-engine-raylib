add_executable(sandbox 
	"src/main.cpp"
	"src/generators/AsteroidGenerator.cpp"
)

# Link the engine library
target_link_libraries(sandbox PRIVATE engine)

# (Optional) Auto-copy assets folder to the build directory so the game can find them
file(COPY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}/bin")