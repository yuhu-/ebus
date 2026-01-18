add_library(ebus Common.cpp Datatypes.cpp  Handler.cpp Request.cpp Sequence.cpp Telegram.cpp TimingStats.cpp) 

add_library(ebus_posix_test Common.cpp Datatypes.cpp Handler.cpp Request.cpp Sequence.cpp Telegram.cpp TimingStats.cpp)
target_compile_definitions(ebus_posix_test PRIVATE -DPOSIX_TEST)