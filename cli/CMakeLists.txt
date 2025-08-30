# Defines the command-line executable target.

add_executable(grad_aff_cli main.cpp)

# Link the CLI against the main grad_aff library, which provides all the
# necessary functions for PBO, PAA, and WRP file handling.
target_link_libraries(grad_aff_cli PRIVATE grad_aff)

# Add a rule to install the compiled executable to the 'bin' directory
# during the installation process.
install(TARGETS grad_aff_cli DESTINATION bin)

