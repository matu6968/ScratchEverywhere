set(SE_DEFAULT_OUTPUT_NAME "scratch") # libscratch.so, loaded by MainActivity (SDLActivity)

option(SE_ANDROID_LEGACY "Use the legacy build for Android 4.4 and older" OFF)

# Pre-KitKat devices (API < 19) get a trimmed "legacy" build, made with an old NDK (r16b is the last with ARMv5/ARMv6 support)
if(ANDROID_PLATFORM_LEVEL LESS 19)
    set(SE_ANDROID_LEGACY TRUE)
else()
    set(SE_ANDROID_LEGACY FALSE)
endif()

# GLES3 needs API 18+; the legacy build only gets SDL's own renderer drivers (GLES1/GLES2/software)
if(SE_ANDROID_LEGACY)
    set(SE_RENDERER_VALID_OPTIONS "sdl2" "headless")
else()
    set(SE_RENDERER_VALID_OPTIONS "opengl_core" "sdl2" "headless")
endif()
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2" "headless")
if(NOT SE_ANDROID_LEGACY)
    set(SE_LUA_BACKEND_VALID_OPTIONS "fallback" "lua51" "luajit")
endif()

if(SE_ANDROID_LEGACY)
    # Pointer caching costs RAM, and these devices have very little of it
    set(SE_CACHING_DEFAULT OFF)
    # SDL 2.28 raised the minimum to API 19, 2.26 is the newest release with a Java side that can work on API 15
    set(CL_SDL2_VERSION "2.26.5")
else()
    set(SE_CACHING_DEFAULT ON)
endif()

set(SE_BUNDLE_DEFAULT ON) # assets live inside libscratch.so, no APK assets needed
set(SE_ALLOW_BUNDLE ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_ALLOW_NATIVE_EXTENSIONS TRUE) # dlopen() is part of bionic

set(SE_PLATFORM_DEFINITIONS "__ANDROID__")
set(SE_PLATFORM "android")

set(SE_HAS_THREADS ON) # pthreads are part of bionic's libc

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE TRUE)
set(SE_HAS_KEYBOARD TRUE)
if(SE_ANDROID_LEGACY)
    # SDL's joystick init opens a file through the Android asset manager, which crashes on Android 4.0.x's Dalvik
    # ("Invalid indirect reference"); a game controller on a device this old isn't worth that anyway.
    set(SE_HAS_CONTROLLER FALSE)
else()
    set(SE_HAS_CONTROLLER TRUE)
endif()

# Everything has to be built for the NDK; the system/package stages would happily
# pick up host libraries through pkg-config when cross-compiling.
set(CL_STAGES toolchain source)

# SDL's Java glue calls JNI functions that live in libSDL2.so; a static SDL gets those
# stripped as unreferenced, so it has to be a shared library on Android.
set(CL_SDL2_REQ_TYPE SHARED)

macro(package_platform)
    target_link_libraries(scratch-everywhere PRIVATE log android)

    # SDL's Java glue has to match the native SDL2 exactly (JNI signatures), so it is copied from the
    # same source tree Catalog downloaded; Gradle compiles it (see android/app/build.gradle).
    # The legacy build uses an older SDL2, so each gets its own copy.
    if(SE_ANDROID_LEGACY)
        set(SE_SDL_JAVA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/sdl-java/legacy")
    else()
        set(SE_SDL_JAVA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/sdl-java/modern")
    endif()
    if(SDL2_SOURCE_DIR)
        file(COPY "${SDL2_SOURCE_DIR}/android-project/app/src/main/java/org/libsdl/app/"
            DESTINATION "${SE_SDL_JAVA_DIR}/org/libsdl/app")
    else()
        message(WARNING "SDL2 was not built from source, SDL's Java glue was not copied into android/")
    endif()
endmacro()
