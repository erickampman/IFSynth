cmake_minimum_required(VERSION 3.21)   # clap-wrapper needs 3.21

# C is needed for DPF's pugl / dgl sources.
project(IFSynth VERSION 0.3.0 LANGUAGES C CXX)

# Everything is linked into the .clap, a shared module, so every static library in it (the engine,
# the plugin core, DPF's DGL) must be position-independent. clang on Linux defaults to PIE and
# hides this; gcc does not ("relocation R_X86_64_PC32 ... recompile with -fPIC"). As PlainOldSynth.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 20)          # PM framework uses std::numbers / std::span
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # clap-wrapper's AUv2 wrapper is Objective-C++ (as are DPF's DistrhoUI_macOS.mm and our
    # BackingScale_mac.mm).
    enable_language(OBJCXX)
endif()

option(IFSYNTH_BUILD_VST3 "Build a VST3 wrapper around the same plugin via clap-wrapper." ON)
option(IFSYNTH_BUILD_AUV2 "Build an AUv2 (.component) wrapper via clap-wrapper (macOS only)." ${APPLE})
option(IFSYNTH_WERROR "Treat warnings as errors (framework + tests)" OFF)
option(IFSYNTH_PRESET_DUMP "Dev-only: press 'P' in the UI to dump the patch as JSON to \$HOME" OFF)

# --- PM framework port -----------------------------------------------------
# Vendored PolygraphModular framework + our shim/MIDI layer, as a static lib. Compiled with
# relaxed warnings (vendored code, its own style); our glue and tests stay strict. PIC so it
# can be linked into the shared .clap module.
add_library(pm_framework STATIC
    src/Common/pm_midi.cpp
    src/Common/pm_param_data.cpp
    src/Module/SinOscillatorModule.cpp
    src/Module/IFSCore.cpp
    src/Module/IFSOscModule.cpp
    src/Module/LPFilterModule.cpp
    src/Module/LevelModule.cpp
    src/Module/IFSEnvModule.cpp
    src/Module/IFSLFOModule.cpp
    src/Module/CVMixerModule.cpp
    src/Module/ADSRHelper.cpp
    src/Module/ADSRModule.cpp
    src/Module/CombineModule.cpp
    src/Module/FinalModule.cpp
    src/Module/SlewModule.cpp
    src/Module/PitchConverterModule.cpp
    src/Module/PitchBendConverterModule.cpp
    src/Module/ChannelPressureModule.cpp
    src/Module/CCConverterModule.cpp
    src/Module/VelocityConverterModule.cpp
    src/Module/ModuleFactory.cpp
    src/Voice/ModuleMap.cpp
    src/Voice/pm_module_specs.cpp
    src/Voice/VoiceStateManager.cpp
    src/Voice/Voice.cpp
    src/Voice/VoiceManager.cpp)
set_target_properties(pm_framework PROPERTIES POSITION_INDEPENDENT_CODE ON)
# All framework folders on the include path so the vendored `#include "X.hpp"` resolve
# across Module/ Voice/ Common/ (mirrors PolygraphModular's layout).
target_include_directories(pm_framework PUBLIC src/Common src/Module src/Voice)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(pm_framework PRIVATE -Wall -Wno-unknown-pragmas -Wno-unused-parameter)
endif()

# --- presets: Presets/*.json compiled in as a C++ array, not loaded at runtime -------------
# Avoids any runtime JSON parser in the plugin and doesn't depend on CLAP's own preset-discovery
# extension — the UI presents its own dropdown. Adding/removing a preset file requires
# reconfiguring (CONFIGURE_DEPENDS re-globs on build, but a brand new file still needs `cmake -B
# build -S .` rerun once to pick up the new match).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB IFSYNTH_PRESET_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Presets/*.json")
set(IFSYNTH_GENERATED_PRESETS_H "${CMAKE_BINARY_DIR}/generated/GeneratedPresets.h")
add_custom_command(
    OUTPUT "${IFSYNTH_GENERATED_PRESETS_H}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/gen_presets.py"
            ${IFSYNTH_PRESET_FILES} -o "${IFSYNTH_GENERATED_PRESETS_H}"
    DEPENDS ${IFSYNTH_PRESET_FILES} "${CMAKE_SOURCE_DIR}/tools/gen_presets.py"
    COMMENT "Generating embedded presets from Presets/*.json")
add_custom_target(generate_presets DEPENDS "${IFSYNTH_GENERATED_PRESETS_H}")

# --- embedded resources: Resources/* compiled in as raw bytes ----------------------------
# Uses DPF's own res2c.py (ships in the dpf/ submodule) to embed the logo PNG's raw file bytes;
# NanoVG's createImageFromMemory() decodes it at UI-construction time. No filesystem dependency
# on the resource file once built — same reasoning as the preset codegen above.
set(IFSYNTH_RESOURCES_HPP "${CMAKE_BINARY_DIR}/generated/EmbeddedResources.hpp")
set(IFSYNTH_RESOURCES_CPP "${CMAKE_BINARY_DIR}/generated/EmbeddedResources.cpp")
file(GLOB IFSYNTH_RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Resources/*")
add_custom_command(
    OUTPUT "${IFSYNTH_RESOURCES_HPP}" "${IFSYNTH_RESOURCES_CPP}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/dpf/utils/res2c.py"
            EmbeddedResources "${CMAKE_SOURCE_DIR}/Resources" "${CMAKE_BINARY_DIR}/generated"
    DEPENDS ${IFSYNTH_RESOURCE_FILES} "${CMAKE_SOURCE_DIR}/dpf/utils/res2c.py"
    COMMENT "Embedding Resources/* as compiled-in data")
add_custom_target(generate_resources DEPENDS "${IFSYNTH_RESOURCES_HPP}" "${IFSYNTH_RESOURCES_CPP}")

# --- DPF is used ONLY for the GUI ------------------------------------------
# UIExporter + NanoVG + pugl (plus the native WebView the Help button needs). The CLAP entry point
# is ours (src/plugin.cpp), because DPF's generated CLAP wrapper cannot receive MIDI 2.0 note
# events — see that file's header.
add_subdirectory(dpf)
# dgl-opengl is normally created by dpf_add_plugin()'s UI_TYPE branch; call the same helper
# directly. SHARED_RESOURCES=TRUE embeds DGL's DejaVu font. USE_WEB_VIEW=TRUE mirrors
# dpf_add_plugin's USE_WEB_VIEW: it defines DGL_USE_WEB_VIEW PUBLIC (required by
# DistrhoPluginChecks.h when DISTRHO_UI_WEB_VIEW is 1) and builds DGL's own WebView impl.
# USE_FILE_BROWSER=TRUE brings in the Distrho String/ChildProcess helpers that impl uses.
dpf__add_dgl_opengl(TRUE TRUE TRUE)
# IFSynth's own namespace (UmpireDAW's docs/decisions.md, "DPF fork"): on macOS pugl names its
# Objective-C classes after it, so two DPF binaries with the same one in a host collide.
target_compile_definitions(dgl-opengl PUBLIC DGL_NAMESPACE=IFSynthDGL)

# --- ifsynth_core: the whole plugin (DSP + host glue + GUI shell) as one static archive ---------
# Shared by the .clap output below. Hidden visibility for our own objects, so clap_entry — marked
# CLAP_EXPORT in plugin.cpp — is the only symbol this target exports (the vendored engine and DGL
# libraries linked in keep their own default visibility).
add_library(ifsynth_core STATIC
    src/plugin.cpp
    src/UI/IFSynthUI.cpp
    "${IFSYNTH_RESOURCES_CPP}"                  # generated: the logo/title bytes (see above)
    "${DPF_ROOT_DIR}/distrho/src/DistrhoUI.cpp"
    "${DPF_ROOT_DIR}/distrho/src/DistrhoUtils.cpp")
set_target_properties(ifsynth_core PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)
target_include_directories(ifsynth_core PUBLIC
    src                               # plugin_gui.h (the DSP <-> GUI seam)
    plugin                            # DistrhoPluginInfo.h + IFSynthParams.h
    external/clap/include             # <clap/clap.h> (the raw entry point)
    "${CMAKE_BINARY_DIR}/generated"   # GeneratedPresets.h + EmbeddedResources.hpp
    "${DPF_ROOT_DIR}/distrho")        # DistrhoUI.hpp / DistrhoUIInternal.hpp
# clap-wrapper's plugin-side extension header only (clapwrapper/vst3.h: which VST3 note expressions to
# offer). Header-only and plain C, so the CLAP build needs it even with IFSYNTH_BUILD_VST3 off.
target_include_directories(ifsynth_core PRIVATE external/clap-wrapper/include)
target_compile_definitions(ifsynth_core PUBLIC
    DISTRHO_PLUGIN_TARGET_CLAP
    DISTRHO_IS_STANDALONE=0
    IFSYNTH_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(ifsynth_core PUBLIC pm_framework dgl-opengl)
add_dependencies(ifsynth_core generate_presets generate_resources)
# TRUE = the UI's Help button opens a native WebView. On macOS this adds DistrhoUI_macOS.mm and
# links WebKit; the WebView implementation itself comes from DPF's UI sources (DistrhoUI_macOS.mm /
# DistrhoUI.cpp / DistrhoUI_win32.cpp), not from dgl.
dpf__add_plugin_specific_ui_sources(ifsynth_core TRUE)
if(APPLE)
    # The live Retina scale of the host window (see the file's header comment).
    target_sources(ifsynth_core PRIVATE src/UI/BackingScale_mac.mm)
endif()
# Dev-only JSON patch dumper (keystroke → file); compiled out of release builds.
if(IFSYNTH_PRESET_DUMP)
    target_compile_definitions(ifsynth_core PRIVATE IFSYNTH_PRESET_DUMP=1)
endif()

# --- the .clap output ------------------------------------------------------
# A MODULE with no sources of its own would be rejected by CMake, and simply linking the archive is
# not enough: nothing references clap_entry, so the linker would drop the object that defines it.
# Hence the empty placeholder plus a whole-archive force-load.
function(ifsynth_force_load_whole_archive TARGET_NAME LIB_NAME)
    if(APPLE)
        target_link_options(${TARGET_NAME} PRIVATE "-Wl,-force_load,$<TARGET_FILE:${LIB_NAME}>")
    elseif(UNIX)
        target_link_options(${TARGET_NAME} PRIVATE
            "-Wl,--whole-archive" "$<TARGET_FILE:${LIB_NAME}>" "-Wl,--no-whole-archive")
    endif()
endfunction()

set(IFSYNTH_EMPTY_SOURCE "${CMAKE_BINARY_DIR}/generated/ifsynth_empty.c")
# A genuinely empty file trips -Wempty-translation-unit under -Wpedantic -Werror, and ISO C requires
# at least one declaration anyway.
set(_ifsynth_empty_content "typedef int ifsynth_empty_translation_unit_marker;\n")
set(_ifsynth_empty_existing "")
if(EXISTS "${IFSYNTH_EMPTY_SOURCE}")
    file(READ "${IFSYNTH_EMPTY_SOURCE}" _ifsynth_empty_existing)
endif()
if(NOT _ifsynth_empty_existing STREQUAL _ifsynth_empty_content)
    file(WRITE "${IFSYNTH_EMPTY_SOURCE}" "${_ifsynth_empty_content}")
endif()

add_library(IFSynth MODULE "${IFSYNTH_EMPTY_SOURCE}")
target_link_libraries(IFSynth PRIVATE ifsynth_core)
ifsynth_force_load_whole_archive(IFSynth ifsynth_core)

if(APPLE)
    set_target_properties(IFSynth PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unlikelyware.ifsynth"
        MACOSX_BUNDLE_BUNDLE_NAME "IFSynth"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
else()
    set_target_properties(IFSynth PROPERTIES PREFIX "" SUFFIX ".clap")
endif()

# --- install: symlink the built .clap into the user CLAP dir ---------------
# Runs automatically after IFSynth is (re)built, so the plugin in the user CLAP dir is always a
# symlink pointing at the freshest build. The rm first clears an old link or a copied bundle
# directory, which create_symlink would not replace.
if(APPLE)
    set(IFSYNTH_CLAP_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
    set(IFSYNTH_CLAP_ARTIFACT "$<TARGET_BUNDLE_DIR:IFSynth>")
else()
    set(IFSYNTH_CLAP_DIR "$ENV{HOME}/.clap")
    set(IFSYNTH_CLAP_ARTIFACT "$<TARGET_FILE:IFSynth>")
endif()
add_custom_command(TARGET IFSynth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IFSYNTH_CLAP_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${IFSYNTH_CLAP_DIR}/IFSynth.clap"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${IFSYNTH_CLAP_ARTIFACT}"
        "${IFSYNTH_CLAP_DIR}/IFSynth.clap"
    COMMENT "Symlinking IFSynth.clap into ${IFSYNTH_CLAP_DIR}"
    VERBATIM)

# --- VST3 / AUv2 via the vendored clap-wrapper, wrapping the very same CLAP ----------------------
# Purely additive: neither block touches the .clap target above. Unlike the .clap, these are not
# installed by every build: run `ninja -C build install-vst3 install-auv2`.
if(IFSYNTH_BUILD_VST3 OR IFSYNTH_BUILD_AUV2)
    if(NOT CLAP_SDK_ROOT)
        set(CLAP_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/clap" CACHE PATH "" FORCE)
    endif()
    add_subdirectory(external/clap-wrapper)
endif()

if(IFSYNTH_BUILD_VST3)
    if(NOT VST3_SDK_ROOT)
        set(VST3_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/vst3sdk" CACHE PATH "" FORCE)
    endif()

    add_library(IFSynth_vst3 MODULE "${IFSYNTH_EMPTY_SOURCE}")
    if(UNIX AND NOT APPLE)
        # clap-wrapper's Linux branch tests a misspelled TCLP_RESOURCE_DIRECTORY, which is never
        # set, so it always warns that RESOURCE_DIRECTORY is unsupported. Defining it empty
        # silences that; we pass no RESOURCE_DIRECTORY.
        set(TCLP_RESOURCE_DIRECTORY "")
    endif()
    target_link_libraries(IFSynth_vst3 PRIVATE ifsynth_core)
    ifsynth_force_load_whole_archive(IFSynth_vst3 ifsynth_core)
    target_add_vst3_wrapper(
        TARGET IFSynth_vst3
        OUTPUT_NAME "IFSynth"
        BUNDLE_IDENTIFIER "com.unlikelyware.ifsynth.vst3"
        BUNDLE_VERSION "${PROJECT_VERSION}")
    if(TARGET base-sdk-vst3 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # The VST3 SDK still uses std::wstring_convert, deprecated since C++17.
        target_compile_options(base-sdk-vst3 PRIVATE -Wno-deprecated-declarations)
    endif()

    if(APPLE)
        set(IFSYNTH_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
        set(IFSYNTH_VST3_ARTIFACT "$<TARGET_BUNDLE_DIR:IFSynth_vst3>")
    else()
        # On Linux $<TARGET_FILE> is the .so inside the bundle; hosts want the bundle folder, which
        # clap-wrapper builds in build/Release (or build/Debug). As Retune's 9a2b6eb.
        set(IFSYNTH_VST3_DIR "$ENV{HOME}/.vst3")
        set(IFSYNTH_VST3_ARTIFACT "${CMAKE_BINARY_DIR}/$<IF:$<CONFIG:Debug>,Debug,Release>/IFSynth.vst3")
    endif()
    add_custom_target(install-vst3
        COMMAND ${CMAKE_COMMAND} -E make_directory "${IFSYNTH_VST3_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${IFSYNTH_VST3_DIR}/IFSynth.vst3"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${IFSYNTH_VST3_ARTIFACT}" "${IFSYNTH_VST3_DIR}/IFSynth.vst3"
        DEPENDS IFSynth_vst3
        VERBATIM
        COMMENT "Symlinking IFSynth.vst3 into ${IFSYNTH_VST3_DIR}")
endif()

if(IFSYNTH_BUILD_AUV2)
    # AUv2 is an Apple-only format, and clap-wrapper's target_add_auv2_wrapper does not exist off
    # Apple platforms. Say so plainly instead of failing with an unknown-command error.
    if(NOT APPLE)
        message(FATAL_ERROR
            "IFSYNTH_BUILD_AUV2 is macOS-only (AUv2 is an Apple plugin format). "
            "Reconfigure with -DIFSYNTH_BUILD_AUV2=OFF.")
    endif()
    # clap-wrapper fetches Apple's AudioUnitSDK itself (CPM) unless pointed at a local copy with
    # -DAUDIOUNIT_SDK_ROOT=...
    if(NOT AUDIOUNIT_SDK_ROOT)
        set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON)
    endif()

    add_library(IFSynth_auv2 MODULE "${IFSYNTH_EMPTY_SOURCE}")
    target_link_libraries(IFSynth_auv2 PRIVATE ifsynth_core)
    ifsynth_force_load_whole_archive(IFSynth_auv2 ifsynth_core)
    # "IFSy" (IFSynth) as the subtype, under Unlikelyware's own "nLKL" manufacturer code.
    target_add_auv2_wrapper(
        TARGET IFSynth_auv2
        OUTPUT_NAME "IFSynth"
        BUNDLE_IDENTIFIER "com.unlikelyware.ifsynth.auv2"
        BUNDLE_VERSION "${PROJECT_VERSION}"
        MANUFACTURER_NAME "Unlikelyware"
        MANUFACTURER_CODE "nLKL"
        SUBTYPE_CODE "IFSy"
        INSTRUMENT_TYPE "aumu")

    # A copy, not a symlink: AU hosts don't reliably find symlinked components.
    set(IFSYNTH_AUV2_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
    add_custom_target(install-auv2
        COMMAND ${CMAKE_COMMAND} -E make_directory "${IFSYNTH_AUV2_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${IFSYNTH_AUV2_DIR}/IFSynth.component"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_BUNDLE_DIR:IFSynth_auv2>" "${IFSYNTH_AUV2_DIR}/IFSynth.component"
        DEPENDS IFSynth_auv2
        VERBATIM
        COMMENT "Copying IFSynth.component into ${IFSYNTH_AUV2_DIR}")
endif()

# --- self-tests (the framework; headless, no DAW) --------------------------
add_executable(pm-selftest test/pm_selftest.cpp)
target_link_libraries(pm-selftest PRIVATE pm_framework)
target_include_directories(pm-selftest PRIVATE external/clap/include) # clap_ump.h decoders

add_executable(pm-module-test test/pm_module_test.cpp)
target_link_libraries(pm-module-test PRIVATE pm_framework)

add_executable(pm-sinosc-test test/pm_sinosc_test.cpp)
target_link_libraries(pm-sinosc-test PRIVATE pm_framework)

add_executable(pm-ifscore-test test/pm_ifscore_test.cpp)
target_link_libraries(pm-ifscore-test PRIVATE pm_framework)

add_executable(pm-adsr-test test/pm_adsr_test.cpp)
target_link_libraries(pm-adsr-test PRIVATE pm_framework)

add_executable(pm-modules-test test/pm_modules_test.cpp)
target_link_libraries(pm-modules-test PRIVATE pm_framework)

add_executable(pm-factory-test test/pm_factory_test.cpp)
target_link_libraries(pm-factory-test PRIVATE pm_framework)

add_executable(pm-voice-test test/pm_voice_test.cpp)
target_link_libraries(pm-voice-test PRIVATE pm_framework)

add_executable(pm-modroute-test test/pm_modroute_test.cpp)
target_link_libraries(pm-modroute-test PRIVATE pm_framework)

add_executable(pm-voicemanager-test test/pm_voicemanager_test.cpp)
target_link_libraries(pm-voicemanager-test PRIVATE pm_framework)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # pm-selftest only pulls in light headers → fully strict (optionally -Werror in CI).
    target_compile_options(pm-selftest PRIVATE -Wall -Wextra)
    if(IFSYNTH_WERROR)
        target_compile_options(pm-selftest PRIVATE -Werror)
    endif()
    # tests that pull in vendored headers → relaxed (unused params in inline methods).
    foreach(t pm-module-test pm-sinosc-test pm-ifscore-test pm-adsr-test pm-modules-test pm-factory-test
              pm-voice-test pm-voicemanager-test pm-modroute-test)
        target_compile_options(${t} PRIVATE -Wall -Wno-unknown-pragmas -Wno-unused-parameter)
    endforeach()
endif()

enable_testing()
add_test(NAME pm COMMAND pm-selftest)
set_tests_properties(pm PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-module COMMAND pm-module-test)
set_tests_properties(pm-module PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-sinosc COMMAND pm-sinosc-test)
set_tests_properties(pm-sinosc PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-ifscore COMMAND pm-ifscore-test)
set_tests_properties(pm-ifscore PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-adsr COMMAND pm-adsr-test)
set_tests_properties(pm-adsr PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-modules COMMAND pm-modules-test)
set_tests_properties(pm-modules PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-factory COMMAND pm-factory-test)
set_tests_properties(pm-factory PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-voice COMMAND pm-voice-test)
set_tests_properties(pm-voice PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-modroute COMMAND pm-modroute-test)
set_tests_properties(pm-modroute PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-voicemanager COMMAND pm-voicemanager-test)
set_tests_properties(pm-voicemanager PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

# --- end-to-end CLAP host test: note-port dialect negotiation + the MIDI 2.0 7.9-pitch path ------
# Drives the real built .clap over the CLAP C API (no DAW, no GUI): dlopen + clap_entry + factory,
# then a MIDI 2.0 channel-voice-2 NoteOn carrying a 7.9-pitch attribute. This is the only check that
# proves the raw entry point does what dpf_add_plugin could not.
add_executable(ifs-clap-host-test test/ifs_clap_host_test.cpp)
target_include_directories(ifs-clap-host-test PRIVATE
    external/clap/include             # <clap/clap.h>
    plugin)                           # IFSynthParams.h (parameter ids/enums)
# dlopen/dlsym/dlclose: on glibc before 2.34 these live in libdl, so ask CMake for the right library
# rather than assuming it is part of libc (it is not, on older Linux distributions).
target_link_libraries(ifs-clap-host-test PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(ifs-clap-host-test IFSynth)

if(APPLE)
    set(IFSYNTH_CLAP_UNDER_TEST "$<TARGET_BUNDLE_DIR:IFSynth>")
else()
    set(IFSYNTH_CLAP_UNDER_TEST "$<TARGET_FILE:IFSynth>")
endif()
add_test(NAME ifs-clap COMMAND ifs-clap-host-test "${IFSYNTH_CLAP_UNDER_TEST}")
set_tests_properties(ifs-clap PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")
