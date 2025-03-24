
set(INC
  ..
)
set(INC_SYS
    ${ZSTD_INCLUDE_DIRS}
)
include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})


set(LIGHTMASS_SRC
    interchange.cpp
    interchange.h
    light.cpp
    light.h
    material.cpp
    material.h
    mesh.cpp
    mesh.h
    scene.cpp
    scene.h
    texture.cpp
    texture.h
    mapping.cpp
    mapping.h
    util.cpp
    util.h
    swarm_impl.cpp
    swarm_impl.h
)

source_group(lightmass FILES ${LIGHTMASS_SRC})
#add_library(cycles_lightmass ${LIGHTMASS_SRC} ${INC_SYS})
cycles_add_library(cycles_lightmass "${LIB}" ${LIGHTMASS_SRC} ${INC_SYS})