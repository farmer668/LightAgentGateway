cmake_minimum_required(VERSION 3.10)
project(LightAgentGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRCS
    Channel.cpp
    Epoll.cpp
    EventLoop.cpp
    EventLoopThread.cpp
    EventLoopThreadPool.cpp
    GeminiProvider.cpp
    GatewayConfig.cpp
    HttpData.cpp
    LightAgentGateway.cpp
    LlmProvider.cpp
    Main.cpp
    MockProvider.cpp
    ProviderFactory.cpp
    Server.cpp
    #ThreadPool.cpp
    Timer.cpp
    Util.cpp
)
include_directories(${PROJECT_SOURCE_DIR}/base)


add_subdirectory(base)

add_executable(WebServer ${SRCS})
target_link_libraries(WebServer libserver_base)


add_subdirectory(tests)
