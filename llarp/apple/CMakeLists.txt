
# 3.13+ so that we can add link libraries to parent targets
cmake_minimum_required(VERSION 3.13)

if (BUILD_SHARED_LIBS OR NOT BUILD_STATIC_DEPS OR NOT STATIC_LINK)
  message(FATAL_ERROR "macOS builds require a full static build; perhaps use the contrib/macos.sh script to build?")
endif()

# god made apple so that man may suffer

find_library(FOUNDATION Foundation REQUIRED)
find_library(NETEXT NetworkExtension REQUIRED)
find_library(COREFOUNDATION CoreFoundation REQUIRED)

target_sources(lokinet-util PRIVATE apple_logger.mm)
target_link_libraries(lokinet-util PUBLIC ${FOUNDATION})

add_executable(lokinet-extension MACOSX_BUNDLE framework.mm)
add_executable(lokinet-dnsproxy MACOSX_BUNDLE dnsproxy.mm)
target_link_libraries(lokinet-extension PUBLIC
  liblokinet
  ${COREFOUNDATION}
  ${NETEXT})
target_link_libraries(lokinet-dnsproxy PUBLIC
  liblokinet
  ${COREFOUNDATION}
  ${NETEXT})

set_target_properties(lokinet-extension PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION appex
  MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/contrib/macos/LokinetExtension.Info.plist.in
  XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
  )

# Not sure what -fapplication-extension does, but XCode puts it in so...
# -e _NSExtensionMain because it has that instead of a `main` function entry point, of course.
target_link_options(lokinet-extension PRIVATE -fapplication-extension -e _NSExtensionMain)
target_compile_options(lokinet-extension PRIVATE -fapplication-extension -fobjc-arc)

add_custom_command(TARGET lokinet-extension
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/macos/lokinet-extension.provisionprofile
  $<TARGET_BUNDLE_DIR:lokinet-extension>/Contents/embedded.provisionprofile
  )

set_target_properties(lokinet-dnsproxy PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION appex
  MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/contrib/macos/LokinetDNSProxy.Info.plist.in
  XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
  )

target_link_options(lokinet-dnsproxy PRIVATE -fapplication-extension -e _NSExtensionMain)
target_compile_options(lokinet-dnsproxy PRIVATE -fapplication-extension -fobjc-arc)

add_custom_command(TARGET lokinet-dnsproxy
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/contrib/macos/lokinet-dnsproxy.provisionprofile
  $<TARGET_BUNDLE_DIR:lokinet-dnsproxy>/Contents/embedded.provisionprofile
  )
