#pragma once

#ifndef NDEBUG
    #include <iostream>
    #define DEBUG_LOG(x) std::cout << x
    #define DEBUG_LOG_ENDL std::endl
#else
    #define DEBUG_LOG(x) ((void)0)
    #define DEBUG_LOG_ENDL ((void)0)
#endif

