#include "Client.hpp"
#include "Server.hpp"
#include "ft_irc.h"

Client::Client(int fd, Server* server)
    : _fd(fd), _nick(""), _user(""), _name(""),
    _authenticated(false), _buffer(""), _server(server) {
  set_hostname(fd);
}

void Client::set_hostname(int client_socket) {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if (getsockname(client_socket, (struct sockaddr *)&addr, &len) == -1) {
    _hostname = "";
    println("getsockname has failed, thus the name is: ");
    printlnnl(_hostname);
    return;
  }
  _hostname = inet_ntoa(addr.sin_addr);
  println("The hostname will be ");
  printlnnl(_hostname);
}

bool Client::read_into_buffer() {
  static char temp[1024];                                     // to automatically zero-initialize
  ssize_t bytes_read = recv(_fd, temp, sizeof(temp) - 1, 0);  // SEGFAULT HERE
  if (bytes_read <= 0) {
    return false;
  }
  _buffer.append(temp, bytes_read);
  println(_buffer);
  return true;
}

bool Client::inviteMember(Client* target, Channel* channel) {
  if (!target || _server->_clients.find(target->get_fd()) == _server->_clients.end()) {
    errorln("The target doesn't belong to this server.");
    return false; 
  }  
  if (!_server->checkForChannel(channel->getName())) {
    _server->send_message(this->_fd, ERR_NOSUCHCHANNEL(channel->getName()));
    return false;
  }
  if (channel->_members.find(target->get_fd()) != channel->_members.end()) {
    _server->send_message(this->_fd, ERR_USERONCHANNEL(target->get_nickname(), channel->getName()));
    return false;
  }
  if (channel->checkChannelModes('i')) {
    if (!channel->isOperator(this)) {
      _server->send_message(this->_fd, ERR_CHANOPRIVSNEEDED(this->_nick, channel->getName()));
      return false;
    }
  } else {
    if (!channel->isMember(this)) {
      _server->send_message(this->_fd, ERR_NOTONCHANNEL(channel->getName()));
      return false;
    }
  }
  _server->send_message(this->_fd ,RPL_INVITING(this->_nick, target->get_nickname(), channel->getName())); 
  _server->send_message(target->get_fd(), INVITE(this->_nick, this->_user, target->get_nickname(), channel->getName()));
	return true;
}

int Client::get_fd() const { return _fd; }

const std::string &Client::get_nickname() const { return _nick; }

const std::string &Client::get_username() const { return _user; }

const std::string &Client::get_realname() const { return _name; }

const std::string &Client::get_hostname() const { return _hostname; }

Server* Client::getServer() const { return _server; }

bool Client::is_authenticated() const { return _authenticated; }

bool Client::password_matched(Server &server) const { return _pass == server.get_pass(); }

const std::string &Client::get_buffer() const { return _buffer; }

const std::string &Client::get_pass() const { return _pass; }

void Client::set_nickname(const std::string &nickname) { _nick = nickname; }

void Client::set_username(const std::string &username) { _user = username; }

void Client::set_realname(const std::string &realname) { _name = realname; }

void Client::set_authentication(bool status) {
  _authenticated = status;
}

void Client::clean_buffer() { _buffer.clear(); }

bool Client::buffer_has_linebreak() { return _buffer.find("\n") != std::string::npos; }

Client::~Client() {}
