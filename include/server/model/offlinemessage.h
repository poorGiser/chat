#ifndef OFFLINEMESSAGE_H
#define OFFLINEMESSAGE_H
#include <string>
class OfflineMessage
{
public:
    OfflineMessage() : id(-1), message("")
    {
    }
    ~OfflineMessage() = default;
    int getId()
    {
        return id;
    }
    std::string getMessage()
    {
        return message;
    }
    void setMessage(std::string message)
    {
        this->message = message;
    }
    void setId(int id)
    {
        this->id = id;
    }

private:
    int id;
    std::string message;
};
#endif