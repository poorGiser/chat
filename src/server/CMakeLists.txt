AUX_SOURCE_DIRECTORY(. DIR_SRCS)
AUX_SOURCE_DIRECTORY(./db DB_SRCS)
AUX_SOURCE_DIRECTORY(./model MODEL_SRCS)
AUX_SOURCE_DIRECTORY(./redis REDIS_SRCS)

add_executable(ChatServer ${DIR_SRCS} ${DB_SRCS} ${MODEL_SRCS} ${REDIS_SRCS})
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)