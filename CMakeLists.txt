cmake_minimum_required(VERSION 3.10)
project(untitled4 C)

# Définir le standard C
set(CMAKE_C_STANDARD 99)

# Options de compilation
if (MSVC)
    # Options pour Visual Studio
    add_compile_options(/W4)
else()
    # Options pour GCC/Clang
    add_compile_options(-Wall -Wextra -O2)
endif()

# Fichiers sources
set(SOURCES
        globals.c
        utils.c
        network.c
        crypto.c
        persistence.c
        ui.c
        main.c
)

# Créer l'exécutable
add_executable(untitled4 ${SOURCES})

# Ajouter les répertoires d'inclusion de libsodium
target_include_directories(untitled4 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/libsodium/include
)

# Lier les bibliothèques
if (WIN32)
    target_link_libraries(untitled4 PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/libsodium/lib/libsodium.a  # ou .dll.a selon votre configuration
            winhttp
            advapi32
            shell32
            gdi32
            user32
    )
else()
    message(FATAL_ERROR "Ce projet est conçu pour Windows uniquement")
endif()

# Configuration de débogage/release
set(CMAKE_BUILD_TYPE Debug)

# Définir les symboles de préprocesseur appropriés
target_compile_definitions(untitled4 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
)

# Post-build command pour copier la DLL libsodium dans le répertoire de sortie si nécessaire
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libsodium.dll")
    add_custom_command(TARGET untitled4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/libsodium.dll"
            "$<TARGET_FILE_DIR:untitled4>/libsodium.dll")
endif()

# Message d'information
message(STATUS "Configuration du projet terminée")