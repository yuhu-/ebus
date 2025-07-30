add_library(ebus Common.cpp Datatypes.cpp  Handler.cpp Request.cpp Sequence.cpp Telegram.cpp) 

add_library(ebus_posix_test Common.cpp Datatypes.cpp Handler.cpp Request.cpp Sequence.cpp Telegram.cpp)
target_compile_definitions(ebus_posix_test PRIVATE -DPOSIX_TEST)