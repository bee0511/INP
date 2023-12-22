#include "header.hpp"

ChatRoom::ChatRoom() = default;

ChatRoom::ChatRoom(const std::string &owner) : owner(owner) {
    clients.clear();
    history.clear();
    pinned_message = std::make_pair("", "");
    filter_list.clear();
    filter_list.push_back("==");
    filter_list.push_back("Superpie");
    filter_list.push_back("hello");
    filter_list.push_back("Starburst Stream");
    filter_list.push_back("Domain Expansion");
}