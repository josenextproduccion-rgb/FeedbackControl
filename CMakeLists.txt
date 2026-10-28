cmake_minimum_required(VERSION 3.22)
project(FeedbackControl VERSION 0.2.0)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(FEEDBACK_CONTROL_FORMATS VST3 AU Standalone)
else()
    set(FEEDBACK_CONTROL_FORMATS VST3 Standalone)
endif()

juce_add_plugin(FeedbackControl
    COMPANY_NAME "FeedbackControl"
    BUNDLE_ID "com.feedbackcontrol.plugin"
    PLUGIN_MANUFACTURER_CODE FbCt
    PLUGIN_CODE Fb01
    FORMATS ${FEEDBACK_CONTROL_FORMATS}
    PRODUCT_NAME "Feedback Control"
    DESCRIPTION "Live audio feedback control prototype"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(FeedbackControl PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_features(FeedbackControl PRIVATE cxx_std_17)
target_link_libraries(FeedbackControl PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_compile_definitions(FeedbackControl PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
