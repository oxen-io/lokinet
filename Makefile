FORMAT = clang-format-9

format:
	$(FORMAT) -i $$(find jni daemon llarp include libabyss pybind | grep -E '\.[h,c](pp)?$$')

format-verify: format
	(type $(FORMAT))
	$(FORMAT) --version
	git diff --quiet || (echo 'Please run make format!!' && git --no-pager diff ; exit 1)
