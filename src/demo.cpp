#include <iostream>
#include <set>
#include <sstream>
#include <fstream>
#include <functional>

#include <cqcppsdk/cqcppsdk.h>

#include "../lib/dllmain.h"

#include <windows.h>

using namespace cq;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

static inline std::pair<bool, int64_t> to_group_or_discuss_id(const GroupID gid)
{
    const bool is_group = gid & (uint64_t(1) << 63);
    return { is_group, gid & ~(uint64_t(1) << 63) };
}

static inline uint64_t to_gid(const int64_t id, const bool is_group)
{
    return is_group ? (id | (uint64_t(1) << 63)) : id;
}

std::string gbk_to_utf8(const char* strGBK)
{
    int len = MultiByteToWideChar(CP_ACP, 0, strGBK, -1, NULL, 0);
    wchar_t* wstr = new wchar_t[len + 1];
    memset(wstr, 0, len + 1);
    MultiByteToWideChar(CP_ACP, 0, strGBK, -1, wstr, len);
    len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = new char[len + 1];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    std::string strTemp = str;
    if(wstr) delete[] wstr;
    if(str) delete[] str;
    return strTemp;
}

std::string utf8_to_gbk(const char* strUTF8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, NULL, 0);
    wchar_t* wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, wszGBK, len);
    len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
    char* szGBK = new char[len + 1];
    memset(szGBK, 0, len + 1);
    WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);
    std::string strTemp(szGBK);
    if(wszGBK) delete[] wszGBK;
    if(szGBK) delete[] szGBK;
    return strTemp;
}

std::pair<bool, std::string> is_at_me(std::string msg)
{
    const std::string at_me_str = MessageSegment::at(get_login_user_id());
    bool is_at_me = false;
    for (size_t i = msg.find(at_me_str); i != std::string::npos; i = msg.find(at_me_str))
    {
        is_at_me = true;
        msg.replace(i, i + at_me_str.size(), " ");
    }
    return { is_at_me, msg };
}

void send_message_spilt_pages(const std::function<void(const std::string&)>& send, const std::string& msg, const uint64_t lines_each_page = 16)
{
    std::stringstream iss(msg);
    for (std::string line; std::getline(iss, line); )
    {
        std::stringstream oss;
        oss << line;
        for (uint64_t i = 1; i < lines_each_page && std::getline(iss, line); ++ i)
        {
            oss << std::endl << line;
        }
        send(oss.str());
    }
}

void lgt_send_private_msg(const UserID& uid, const char* gbk_msg)
{
    std::string msg = gbk_to_utf8(gbk_msg);
    try
    {
        send_message_spilt_pages(std::bind(send_private_message, uid, std::placeholders::_1), msg);
    }
    catch (ApiError &e) {}
}

void lgt_send_public_msg(const GroupID& gid, const char* gbk_msg)
{
    std::string msg = gbk_to_utf8(gbk_msg);
    const auto [is_group, id] = to_group_or_discuss_id(gid);
    try
    {
        send_message_spilt_pages(std::bind(is_group ? send_group_message : send_discuss_message, id, std::placeholders::_1), msg);
    }
    catch (ApiError &e) {}
}

void lgt_at(const UserID& uid, char* buf, const uint64_t len)
{
    std::string at_str = MessageSegment::at(uid);
    strcpy_s(buf, len, at_str.c_str());
}

CQ_INIT {
    on_enable([]
    {
        BOT_API::Init(get_login_user_id(), lgt_send_private_msg, lgt_send_public_msg, lgt_at);
    });

    on_private_message([](const PrivateMessageEvent &e)
    {
        BOT_API::HandlePrivateRequest(e.user_id, utf8_to_gbk(e.message.c_str()).c_str());
    });

    on_group_message([](const GroupMessageEvent &e)
    {
        const auto& [ok, msg] = is_at_me(utf8_to_gbk(e.message.c_str()));
        if (ok) { BOT_API::HandlePublicRequest(e.user_id, to_gid(e.group_id, true), msg.c_str()); }
        
    });

    on_discuss_message([](const DiscussMessageEvent &e)
    {
        const auto& [ok, msg] = is_at_me(utf8_to_gbk(e.message.c_str()));
        if (ok) { BOT_API::HandlePublicRequest(e.user_id, to_gid(e.discuss_id, true), msg.c_str()); }
    });
}
