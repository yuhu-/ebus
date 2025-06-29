add_library(ebus Common.cpp Sequence.cpp Telegram.cpp Datatypes.cpp Handler.cpp) 

add_library(ebus_posix_test Common.cpp Sequence.cpp Telegram.cpp Datatypes.cpp Handler.cpp)
target_compile_definitions(ebus_posix_test PRIVATE -DPOSIX_TEST)