add_executable(cex_type1_service main.cpp)
target_link_libraries(cex_type1_service PRIVATE cex_type1 common spdlog::spdlog Poco::Net Poco::Util Poco::Foundation)
