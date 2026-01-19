add_executable(sandbox 
    src/main.cpp
)

# Link against your Engine library
target_link_libraries(sandbox PRIVATE engine)

# Copy assets if you have them (Optional)
file(COPY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}")