ifeq ($(OS),Windows_NT)
    ifeq ($(shell uname -o),Cygwin)
        UNIX_ENV := true
    else
        WIN_ENV := true
    endif
else
    UNIX_ENV := true
endif
