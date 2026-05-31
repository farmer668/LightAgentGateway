cmake_minimum_required(VERSION 3.10)
project(LightAgentGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

set(SRCS
    Channel.cpp
    Epoll.cpp
    EventLoop.cpp
    EventLoopThread.cpp
    EventLoopThreadPool.cpp
    GeminiProvider.cpp
    GatewayConfig.cpp
    HttpClient.cpp
    HttpData.cpp
    JsonUtil.cpp
    KnowledgeBase.cpp
    LightAgentGateway.cpp
    LlmProvider.cpp
    Main.cpp
    MockProvider.cpp
    OllamaProvider.cpp
    ProviderFactory.cpp
    RagEngine.cpp
    Server.cpp
    StreamUtil.cpp
    #ThreadPool.cpp
    Timer.cpp
    Util.cpp
)
include_directories(${PROJECT_SOURCE_DIR}/base ${CURL_INCLUDE_DIRS})


add_subdirectory(base)

add_executable(WebServer ${SRCS})
target_link_libraries(WebServer libserver_base ${CURL_LIBRARIES})


add_subdirectory(tests)
