set(CXX_COMPAT_SRC
  cppbackport-master/lib/fs/rename.cpp
  cppbackport-master/lib/fs/filestatus.cpp
  cppbackport-master/lib/fs/filetype.cpp
  cppbackport-master/lib/fs/cleanpath.cpp
  cppbackport-master/lib/fs/perms.cpp
  cppbackport-master/lib/fs/equivalent.cpp
  cppbackport-master/lib/fs/current_path.cpp
  cppbackport-master/lib/fs/basename.cpp
  cppbackport-master/lib/fs/tempdir.cpp
  cppbackport-master/lib/fs/create_directory.cpp
  cppbackport-master/lib/fs/path.cpp
  cppbackport-master/lib/fs/remove.cpp
  cppbackport-master/lib/fs/diriter.cpp
  cppbackport-master/lib/fs/copyfile.cpp
  cppbackport-master/lib/fs/absolute.cpp
  cppbackport-master/lib/fs/direntry.cpp
)

add_library(cppbackport OBJECT ${CXX_COMPAT_SRC})
target_include_directories(cppbackport PUBLIC cppbackport-master/lib)
