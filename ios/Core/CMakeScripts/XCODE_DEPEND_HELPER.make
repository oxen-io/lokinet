# DO NOT EDIT
# This makefile makes sure all linkable targets are
# up-to-date with anything they link to
default:
	echo "Do not invoke directly"

# Rules to remove targets that are older than anything to which they
# link.  This forces Xcode to relink the targets from scratch.  It
# does not seem to check these dependencies itself.
PostBuild.absl_any.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a


PostBuild.absl_bad_any_cast.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a


PostBuild.absl_bad_optional_access.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a


PostBuild.absl_base.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a


PostBuild.absl_container.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a


PostBuild.absl_demangle_internal.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a


PostBuild.absl_dynamic_annotations.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a


PostBuild.absl_failure_signal_handler.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a


PostBuild.absl_hash.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a


PostBuild.absl_int128.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a


PostBuild.absl_internal_city.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a


PostBuild.absl_internal_debugging_internal.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a


PostBuild.absl_internal_examine_stack.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a


PostBuild.absl_internal_malloc_internal.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a


PostBuild.absl_internal_spinlock_wait.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a


PostBuild.absl_internal_throw_delegate.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a


PostBuild.absl_leak_check.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a


PostBuild.absl_leak_check_disable.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a


PostBuild.absl_meta.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a


PostBuild.absl_numeric.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a


PostBuild.absl_optional.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a


PostBuild.absl_raw_hash_set.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a


PostBuild.absl_span.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a


PostBuild.absl_stacktrace.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a


PostBuild.absl_str_format.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a


PostBuild.absl_strings.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a


PostBuild.absl_symbolize.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a


PostBuild.absl_synchronization.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a


PostBuild.absl_time.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a


PostBuild.absl_utility.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a


PostBuild.absl_variant.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a


PostBuild.abyss.Debug:
/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/libabyss.a


PostBuild.abyss-main.Debug:
PostBuild.abyss.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-platform.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-cryptography.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-util.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_synchronization.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_symbolize.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_demangle_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_malloc_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_time.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_stacktrace.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_debugging_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.nlohmann_json.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_hash.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_optional.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_bad_optional_access.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_variant.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_strings.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_fixed_array.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_memory.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_meta.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_throw_delegate.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_compressed_tuple.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_utility.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_base.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_base_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_dynamic_annotations.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_spinlock_wait.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_algorithm.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_int128.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_city.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_endian.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_core_headers.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_config.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.cppbackport.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.libutp.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main:\
	/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main


PostBuild.cppbackport.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a


PostBuild.gmock.Debug:
/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgmockd.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Debug/libgmockd.a


PostBuild.gmock_main.Debug:
/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgmock_maind.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Debug/libgmock_maind.a


PostBuild.gtest.Debug:
/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgtestd.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Debug/libgtestd.a


PostBuild.gtest_main.Debug:
/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgtest_maind.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Debug/libgtest_maind.a


PostBuild.libutp.Debug:
/Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a


PostBuild.lokinet.Debug:
PostBuild.lokinet-static.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.abyss.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-platform.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-util.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_synchronization.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_symbolize.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_demangle_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_malloc_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_time.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_stacktrace.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_debugging_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.nlohmann_json.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_hash.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_optional.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_bad_optional_access.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_variant.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_strings.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_fixed_array.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_memory.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_meta.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_throw_delegate.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_compressed_tuple.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_utility.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_base.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_base_internal.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_dynamic_annotations.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_spinlock_wait.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_algorithm.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_int128.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_city.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_endian.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_core_headers.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_config.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-cryptography.Debug: /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet:\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet


PostBuild.lokinet-cryptography.Debug:
/Users/niels/Code/loki-network/ios/Core/crypto/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/crypto/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a


PostBuild.lokinet-platform.Debug:
/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a


PostBuild.lokinet-static.Debug:
/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a


PostBuild.lokinet-util.Debug:
/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a


PostBuild.pow10_helper.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a


PostBuild.str_format_extension_internal.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a


PostBuild.str_format_internal.Debug:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a


PostBuild.testAll.Debug:
PostBuild.gmock.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.gtest.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-static.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.abyss.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-platform.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-util.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_synchronization.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_symbolize.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_demangle_internal.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_malloc_internal.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_time.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_stacktrace.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_debugging_internal.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.nlohmann_json.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_hash.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_optional.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_bad_optional_access.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_strings.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_fixed_array.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_memory.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_meta.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_throw_delegate.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_compressed_tuple.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_utility.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_base.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_base_internal.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_dynamic_annotations.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_spinlock_wait.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_algorithm.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_int128.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_city.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_endian.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_core_headers.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_config.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.cppbackport.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-cryptography.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.libutp.Debug: /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
/Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll:\
	/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgmockd.a\
	/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgtestd.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/test/Debug${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll


PostBuild.absl_any.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a


PostBuild.absl_bad_any_cast.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a


PostBuild.absl_bad_optional_access.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a


PostBuild.absl_base.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a


PostBuild.absl_container.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a


PostBuild.absl_demangle_internal.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a


PostBuild.absl_dynamic_annotations.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a


PostBuild.absl_failure_signal_handler.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a


PostBuild.absl_hash.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a


PostBuild.absl_int128.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a


PostBuild.absl_internal_city.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a


PostBuild.absl_internal_debugging_internal.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a


PostBuild.absl_internal_examine_stack.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a


PostBuild.absl_internal_malloc_internal.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a


PostBuild.absl_internal_spinlock_wait.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a


PostBuild.absl_internal_throw_delegate.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a


PostBuild.absl_leak_check.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a


PostBuild.absl_leak_check_disable.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a


PostBuild.absl_meta.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a


PostBuild.absl_numeric.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a


PostBuild.absl_optional.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a


PostBuild.absl_raw_hash_set.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a


PostBuild.absl_span.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a


PostBuild.absl_stacktrace.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a


PostBuild.absl_str_format.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a


PostBuild.absl_strings.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a


PostBuild.absl_symbolize.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a


PostBuild.absl_synchronization.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a


PostBuild.absl_time.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a


PostBuild.absl_utility.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a


PostBuild.absl_variant.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a


PostBuild.abyss.Release:
/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/libabyss.a


PostBuild.abyss-main.Release:
PostBuild.abyss.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-platform.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-cryptography.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-util.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_synchronization.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_symbolize.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_demangle_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_malloc_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_time.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_stacktrace.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_debugging_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.nlohmann_json.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_hash.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_optional.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_bad_optional_access.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_variant.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_strings.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_fixed_array.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_memory.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_meta.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_throw_delegate.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_compressed_tuple.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_utility.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_base.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_base_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_dynamic_annotations.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_spinlock_wait.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_algorithm.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_int128.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_city.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_endian.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_core_headers.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_config.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.cppbackport.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.libutp.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main:\
	/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main


PostBuild.cppbackport.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a


PostBuild.gmock.Release:
/Users/niels/Code/loki-network/ios/Core/lib/Release/libgmock.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Release/libgmock.a


PostBuild.gmock_main.Release:
/Users/niels/Code/loki-network/ios/Core/lib/Release/libgmock_main.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Release/libgmock_main.a


PostBuild.gtest.Release:
/Users/niels/Code/loki-network/ios/Core/lib/Release/libgtest.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Release/libgtest.a


PostBuild.gtest_main.Release:
/Users/niels/Code/loki-network/ios/Core/lib/Release/libgtest_main.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/Release/libgtest_main.a


PostBuild.libutp.Release:
/Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a


PostBuild.lokinet.Release:
PostBuild.lokinet-static.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.abyss.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-platform.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-util.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_synchronization.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_symbolize.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_demangle_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_malloc_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_time.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_stacktrace.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_debugging_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.nlohmann_json.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_hash.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_optional.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_bad_optional_access.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_variant.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_strings.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_fixed_array.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_memory.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_meta.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_throw_delegate.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_compressed_tuple.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_utility.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_base.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_base_internal.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_dynamic_annotations.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_spinlock_wait.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_algorithm.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_int128.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_city.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_endian.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_core_headers.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_config.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-cryptography.Release: /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet:\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet


PostBuild.lokinet-cryptography.Release:
/Users/niels/Code/loki-network/ios/Core/crypto/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/crypto/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a


PostBuild.lokinet-platform.Release:
/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a


PostBuild.lokinet-static.Release:
/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a


PostBuild.lokinet-util.Release:
/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a


PostBuild.pow10_helper.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a


PostBuild.str_format_extension_internal.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a


PostBuild.str_format_internal.Release:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a


PostBuild.testAll.Release:
PostBuild.gmock.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.gtest.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-static.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.abyss.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-platform.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-util.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_synchronization.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_symbolize.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_demangle_internal.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_malloc_internal.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_time.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_stacktrace.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_debugging_internal.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.nlohmann_json.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_hash.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_optional.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_bad_optional_access.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_strings.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_fixed_array.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_memory.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_meta.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_throw_delegate.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_compressed_tuple.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_utility.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_base.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_base_internal.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_dynamic_annotations.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_spinlock_wait.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_algorithm.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_int128.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_city.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_endian.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_core_headers.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_config.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.cppbackport.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-cryptography.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.libutp.Release: /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
/Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll:\
	/Users/niels/Code/loki-network/ios/Core/lib/Release/libgmock.a\
	/Users/niels/Code/loki-network/ios/Core/lib/Release/libgtest.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/test/Release${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll


PostBuild.absl_any.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a


PostBuild.absl_bad_any_cast.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a


PostBuild.absl_bad_optional_access.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a


PostBuild.absl_base.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a


PostBuild.absl_container.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a


PostBuild.absl_demangle_internal.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a


PostBuild.absl_dynamic_annotations.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a


PostBuild.absl_failure_signal_handler.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a


PostBuild.absl_hash.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a


PostBuild.absl_int128.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a


PostBuild.absl_internal_city.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a


PostBuild.absl_internal_debugging_internal.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a


PostBuild.absl_internal_examine_stack.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a


PostBuild.absl_internal_malloc_internal.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a


PostBuild.absl_internal_spinlock_wait.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a


PostBuild.absl_internal_throw_delegate.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a


PostBuild.absl_leak_check.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a


PostBuild.absl_leak_check_disable.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a


PostBuild.absl_meta.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a


PostBuild.absl_numeric.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a


PostBuild.absl_optional.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a


PostBuild.absl_raw_hash_set.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a


PostBuild.absl_span.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a


PostBuild.absl_stacktrace.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a


PostBuild.absl_str_format.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a


PostBuild.absl_strings.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a


PostBuild.absl_symbolize.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a


PostBuild.absl_synchronization.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a


PostBuild.absl_time.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a


PostBuild.absl_utility.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a


PostBuild.absl_variant.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a


PostBuild.abyss.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabyss.a


PostBuild.abyss-main.MinSizeRel:
PostBuild.abyss.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-platform.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-cryptography.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-util.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_synchronization.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_symbolize.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_demangle_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_malloc_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_time.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_stacktrace.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_debugging_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.nlohmann_json.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_hash.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_optional.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_bad_optional_access.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_variant.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_strings.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_fixed_array.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_memory.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_meta.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_throw_delegate.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_compressed_tuple.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_utility.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_base.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_base_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_dynamic_annotations.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_spinlock_wait.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_algorithm.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_int128.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_city.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_endian.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_core_headers.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_config.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.cppbackport.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.libutp.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main:\
	/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main


PostBuild.cppbackport.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a


PostBuild.gmock.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgmock.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgmock.a


PostBuild.gmock_main.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgmock_main.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgmock_main.a


PostBuild.gtest.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgtest.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgtest.a


PostBuild.gtest_main.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgtest_main.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgtest_main.a


PostBuild.libutp.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a


PostBuild.lokinet.MinSizeRel:
PostBuild.lokinet-static.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.abyss.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-platform.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-util.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_synchronization.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_symbolize.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_demangle_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_malloc_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_time.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_stacktrace.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_debugging_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.nlohmann_json.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_hash.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_optional.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_bad_optional_access.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_variant.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_strings.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_fixed_array.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_memory.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_meta.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_throw_delegate.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_compressed_tuple.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_utility.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_base.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_base_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_dynamic_annotations.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_spinlock_wait.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_algorithm.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_int128.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_city.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_endian.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_core_headers.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_config.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-cryptography.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet:\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet


PostBuild.lokinet-cryptography.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/crypto/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/crypto/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a


PostBuild.lokinet-platform.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a


PostBuild.lokinet-static.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a


PostBuild.lokinet-util.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a


PostBuild.pow10_helper.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a


PostBuild.str_format_extension_internal.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a


PostBuild.str_format_internal.MinSizeRel:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a


PostBuild.testAll.MinSizeRel:
PostBuild.gmock.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.gtest.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-static.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.abyss.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-platform.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-util.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_synchronization.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_symbolize.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_demangle_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_malloc_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_time.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_stacktrace.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_debugging_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.nlohmann_json.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_hash.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_optional.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_bad_optional_access.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_strings.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_fixed_array.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_memory.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_meta.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_throw_delegate.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_compressed_tuple.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_utility.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_base.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_base_internal.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_dynamic_annotations.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_spinlock_wait.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_algorithm.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_int128.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_city.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_endian.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_core_headers.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_config.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.cppbackport.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-cryptography.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.libutp.MinSizeRel: /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
/Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll:\
	/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgmock.a\
	/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgtest.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/test/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll


PostBuild.absl_any.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_any.a


PostBuild.absl_bad_any_cast.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_any_cast.a


PostBuild.absl_bad_optional_access.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a


PostBuild.absl_base.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a


PostBuild.absl_container.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_container.a


PostBuild.absl_demangle_internal.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a


PostBuild.absl_dynamic_annotations.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a


PostBuild.absl_failure_signal_handler.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_failure_signal_handler.a


PostBuild.absl_hash.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a


PostBuild.absl_int128.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a


PostBuild.absl_internal_city.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a


PostBuild.absl_internal_debugging_internal.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a


PostBuild.absl_internal_examine_stack.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_examine_stack.a


PostBuild.absl_internal_malloc_internal.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a


PostBuild.absl_internal_spinlock_wait.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a


PostBuild.absl_internal_throw_delegate.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a


PostBuild.absl_leak_check.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check.a


PostBuild.absl_leak_check_disable.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_leak_check_disable.a


PostBuild.absl_meta.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a


PostBuild.absl_numeric.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_numeric.a


PostBuild.absl_optional.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a


PostBuild.absl_raw_hash_set.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/container/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_raw_hash_set.a


PostBuild.absl_span.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_span.a


PostBuild.absl_stacktrace.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a


PostBuild.absl_str_format.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_str_format.a


PostBuild.absl_strings.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a


PostBuild.absl_symbolize.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a


PostBuild.absl_synchronization.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a


PostBuild.absl_time.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a


PostBuild.absl_utility.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a


PostBuild.absl_variant.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a


PostBuild.abyss.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabyss.a


PostBuild.abyss-main.RelWithDebInfo:
PostBuild.abyss.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-platform.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-cryptography.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.lokinet-util.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_synchronization.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_symbolize.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_demangle_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_malloc_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_time.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_stacktrace.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_debugging_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.nlohmann_json.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_hash.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_optional.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_bad_optional_access.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_variant.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_strings.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_fixed_array.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_memory.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_meta.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_throw_delegate.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_compressed_tuple.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_utility.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_base.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_base_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_dynamic_annotations.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_spinlock_wait.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_algorithm.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_int128.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_internal_city.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_endian.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_core_headers.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.absl_config.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.cppbackport.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
PostBuild.libutp.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main
/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main:\
	/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/abyss-main.app/abyss-main


PostBuild.cppbackport.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a


PostBuild.gmock.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgmock.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgmock.a


PostBuild.gmock_main.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgmock_main.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgmock_main.a


PostBuild.gtest.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgtest.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgtest.a


PostBuild.gtest_main.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgtest_main.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgtest_main.a


PostBuild.libutp.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a


PostBuild.lokinet.RelWithDebInfo:
PostBuild.lokinet-static.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.abyss.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-platform.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.libutp.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-util.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.cppbackport.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_synchronization.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_symbolize.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_demangle_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_malloc_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_time.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_stacktrace.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_debugging_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.nlohmann_json.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_hash.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_optional.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_bad_optional_access.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_variant.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_strings.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_fixed_array.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_memory.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_meta.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_throw_delegate.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_compressed_tuple.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_utility.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_base.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_base_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_dynamic_annotations.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_spinlock_wait.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_algorithm.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_int128.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_internal_city.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_endian.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_core_headers.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.absl_config.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
PostBuild.lokinet-cryptography.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet
/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet:\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/lokinet.app/lokinet


PostBuild.lokinet-cryptography.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/crypto/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/crypto/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a


PostBuild.lokinet-platform.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a


PostBuild.lokinet-static.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a


PostBuild.lokinet-util.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a


PostBuild.pow10_helper.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libpow10_helper.a


PostBuild.str_format_extension_internal.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libstr_format_extension_internal.a


PostBuild.str_format_internal.RelWithDebInfo:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a:
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libstr_format_internal.a


PostBuild.testAll.RelWithDebInfo:
PostBuild.gmock.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.gtest.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-static.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.abyss.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-platform.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-util.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_synchronization.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_symbolize.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_demangle_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_malloc_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_time.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_stacktrace.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_debugging_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.nlohmann_json.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_hash.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_variant.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_optional.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_bad_optional_access.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_strings.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_fixed_array.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_memory.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_meta.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_throw_delegate.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_compressed_tuple.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_utility.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_base.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_base_internal.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_dynamic_annotations.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_spinlock_wait.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_algorithm.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_int128.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_internal_city.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_endian.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_core_headers.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.absl_config.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.cppbackport.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.lokinet-cryptography.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
PostBuild.libutp.RelWithDebInfo: /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll
/Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll:\
	/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgmock.a\
	/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgtest.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabyss.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a\
	/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a\
	/Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a\
	/Users/niels/Code/loki-network/ios/Core/crypto/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a\
	/Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a
	/bin/rm -f /Users/niels/Code/loki-network/ios/Core/test/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/testAll.app/testAll




# For each target create a dummy ruleso the target does not have to exist
/Users/niels/Code/loki-network/ios/Core/Debug${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
/Users/niels/Code/loki-network/ios/Core/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
/Users/niels/Code/loki-network/ios/Core/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
/Users/niels/Code/loki-network/ios/Core/Release${EFFECTIVE_PLATFORM_NAME}/libabyss.a:
/Users/niels/Code/loki-network/ios/Core/crypto/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
/Users/niels/Code/loki-network/ios/Core/crypto/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
/Users/niels/Code/loki-network/ios/Core/crypto/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
/Users/niels/Code/loki-network/ios/Core/crypto/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-cryptography.a:
/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgmockd.a:
/Users/niels/Code/loki-network/ios/Core/lib/Debug/libgtestd.a:
/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgmock.a:
/Users/niels/Code/loki-network/ios/Core/lib/MinSizeRel/libgtest.a:
/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgmock.a:
/Users/niels/Code/loki-network/ios/Core/lib/RelWithDebInfo/libgtest.a:
/Users/niels/Code/loki-network/ios/Core/lib/Release/libgmock.a:
/Users/niels/Code/loki-network/ios/Core/lib/Release/libgtest.a:
/Users/niels/Code/loki-network/ios/Core/libutp/Debug${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
/Users/niels/Code/loki-network/ios/Core/libutp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
/Users/niels/Code/loki-network/ios/Core/libutp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
/Users/niels/Code/loki-network/ios/Core/libutp/Release${EFFECTIVE_PLATFORM_NAME}/liblibutp.a:
/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
/Users/niels/Code/loki-network/ios/Core/llarp/Debug${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
/Users/niels/Code/loki-network/ios/Core/llarp/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
/Users/niels/Code/loki-network/ios/Core/llarp/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-platform.a:
/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-static.a:
/Users/niels/Code/loki-network/ios/Core/llarp/Release${EFFECTIVE_PLATFORM_NAME}/liblokinet-util.a:
/Users/niels/Code/loki-network/ios/Core/vendor/Debug${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
/Users/niels/Code/loki-network/ios/Core/vendor/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
/Users/niels/Code/loki-network/ios/Core/vendor/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
/Users/niels/Code/loki-network/ios/Core/vendor/Release${EFFECTIVE_PLATFORM_NAME}/libcppbackport.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_base.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_dynamic_annotations.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_malloc_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_spinlock_wait.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/base/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_throw_delegate.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_demangle_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_debugging_internal.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_stacktrace.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/debugging/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_symbolize.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_hash.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/hash/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_internal_city.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/meta/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_meta.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/numeric/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_int128.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/strings/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_strings.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/synchronization/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_synchronization.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/time/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_time.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_bad_optional_access.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_optional.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/types/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_variant.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Debug${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
/Users/niels/Code/loki-network/ios/Core/vendor/abseil-cpp/absl/utility/Release${EFFECTIVE_PLATFORM_NAME}/libabsl_utility.a:
