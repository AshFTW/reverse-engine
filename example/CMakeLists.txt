# ┌──────────────────────────────────────────────────────────────────┐
# │  Project settings                                                │
# └──────────────────────────────────────────────────────────────────┘
project(example)

# ┌──────────────────────────────────────────────────────────────────┐
# │  Build executable                                                │
# └──────────────────────────────────────────────────────────────────┘
add_executable(trainer trainer.cc)

set_property(TARGET trainer PROPERTY CXX_STANDARD 20)

target_link_libraries(trainer
        reverseengine
        pthread
        boost_iostreams
        )

# ┌──────────────────────────────────────────────────────────────────┐
# │  Build executable                                                │
# └──────────────────────────────────────────────────────────────────┘
add_executable(csgo_linux64_fix_resources_download csgo_linux64/fix_resources_download/main.cc)

set_property(TARGET csgo_linux64_fix_resources_download PROPERTY CXX_STANDARD 20)

target_link_libraries(csgo_linux64_fix_resources_download
        reverseengine
        pthread
        curl
        boost_iostreams
        )
