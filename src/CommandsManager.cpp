#include <string>
#include "Client.hpp"
#include "CommandsManager.hpp"
#include "ft_irc.h"
#include <iostream>

CmdFunc CommandsManager::command_handlers[UNKNOWN + 1] = {
  &CommandsManager::privmsg, &CommandsManager::join, &CommandsManager::nick,
  &CommandsManager::user, &CommandsManager::quit, &CommandsManager::kick,
  &CommandsManager::pass, &CommandsManager::invite, &CommandsManager::topic,
  &CommandsManager::mode, &CommandsManager::unknown
};

CommandsManager::CommandsManager(Server &server) : server(server) {}

bool CommandsManager::requiresRegistration(CommandType type) {
    switch (type) {
        case PRIVMSG:
        case JOIN:
        case QUIT:
        case KICK:
        case INVITE:
        case TOPIC:
        case MODE:
            return true;
        default:
            return false; 
    }
}

bool not_fully_registered(Client &sender) {
    return (!sender.is_authenticated() || sender.get_nickname().empty() || sender.get_username().empty());
}

void CommandsManager::execute(Commands &commands) {
  Client &sender = commands.get_sender();
  const std::list<Command> &cmd_list = commands.get_list();

  for (std::list<Command>::const_iterator it = cmd_list.begin(); it != cmd_list.end(); ++it) {
    const Command &cmd = *it;
    if (requiresRegistration(cmd.type) && not_fully_registered(sender)) {
      server.send_message(sender.get_fd(), ERR_NOTREGISTERED());
      continue;
    }
    (this->*(command_handlers[cmd.type]))(commands, cmd);
  }
  commands.clear();
}

void CommandsManager::privmsg(Commands &commands, const Command &cmd) {
    Client &sender = commands.get_sender();
    
    if (!sender.is_authenticated()) {
        server.send_message(sender.get_fd(), ERR_NOTREGISTERED());
        return;
    }

    if (cmd.parameters.empty()) {
        server.send_message(sender.get_fd(), ERR_NORECIPIENT(sender.get_nickname()));
        return;
    }
    if (cmd.parameters.size() < 2) {
        server.send_message(sender.get_fd(), ERR_NOTEXTTOSEND(sender.get_nickname()));
        return;
    }

    const std::string &recipient = cmd.parameters[0];
    const std::string &message = cmd.parameters[1];

    println("Recipient: " << recipient[0]);
    if (recipient[0] == '#') {        
        if (server.checkForChannel(recipient)) {
            Channel* channel = server._channels[recipient];
            println("Channel found: " << channel->getName());
            if (!channel->isMember(&sender)) {
                server.send_message(sender.get_fd(), ERR_CANNOTSENDTOCHAN(recipient));
                return;
            }

            for (std::map<int, Client*>::const_iterator it = channel->getMembers().begin(); it != channel->getMembers().end(); ++it) {
                Client* member = it->second;
                if (member != &sender) {
                    server.send_message(member->get_fd(), RPL_PRIVMSG(sender.get_client_identifier(), recipient, message));
                }
            }
        } else {
            server.send_message(sender.get_fd(), ERR_NOSUCHCHANNEL(recipient));
        }
    } else {
        bool userFound = false;
        for (std::map<int, Client*>::iterator it = server._clients.begin(); it != server._clients.end(); ++it) {
            Client* client = it->second;
            if (client->get_nickname() == recipient) {
                server.send_message(client->get_fd(), RPL_PRIVMSG(sender.get_client_identifier(), recipient, message));
                userFound = true;
                break;
            }
        }
        if (!userFound) {
            server.send_message(sender.get_fd(), ERR_NOSUCHNICK(recipient));
        }
    }
}


void CommandsManager::join(Commands &commands, const Command &cmd) {
    Channel* channel;
    Client &sender = commands.get_sender();
    std::string channel_name = cmd.parameters[0];

    if (server.checkForChannel(channel_name)) {
        channel = server._channels[channel_name];
        if (!channel->isOperator(&sender)) {
            if (channel->checkChannelModes('l') && channel->getCurrentMembersCount() < channel->getUserLimit()) {
                channel->addMember(&sender);
                server.send_message(sender.get_fd(), RPL_JOIN(sender.get_client_identifier(), channel_name));
                if (!channel->getTopic().empty()) {
                    server.send_message(sender.get_fd(), RPL_TOPIC(sender.get_client_identifier(), channel_name, channel->getTopic()));
                }
            }
            else if (channel->checkChannelModes('l')) {
                server.send_message(sender.get_fd(), ERR_CHANNELISFULL(channel_name));
            }
            else {
                channel->addMember(&sender);
            }
        }
        return ;
    }

    Channel created_channel(channel_name, &sender);
    channel = &created_channel;

    if (server._channels.find(channel_name) != server._channels.end()) {
        std::cout << "Channel created successfully: " << channel_name << std::endl;
    } else {
        std::cout << "Failed to create channel: " << channel_name << std::endl;
    }
    std::string names;

    for (std::map<int, Client*>::const_iterator it = channel->getMembers().begin(); it != channel->getMembers().end(); ++it) {
        Client* member = it->second;
        if (member != &sender) {
            server.send_message(member->get_fd(), RPL_JOIN(sender.get_client_identifier(), channel_name));
            if (channel->isOperator(member))
                names += "@";
            names += member->get_nickname() + " ";
        }
    }

    names += "@" + sender.get_nickname();
    server.send_message(sender.get_fd(), RPL_JOIN(sender.get_username(), channel_name));
    if (!channel->getTopic().empty()) {
        server.send_message(sender.get_fd(), RPL_TOPIC(sender.get_nickname(), channel_name, channel->getTopic()));
    }
    server.send_message(sender.get_fd(), RPL_NAMREPLY(sender.get_nickname(), channel_name, names));
    server.send_message(sender.get_fd(), RPL_ENDOFNAMES(sender.get_nickname(), channel_name)); 
}


void CommandsManager::nick(Commands &commands, const Command &cmd) {
    Client &sender = commands.get_sender();
    const std::string &param = cmd.parameters.empty() ? "" : cmd.parameters[0];
    if (param.empty()) {
        server.send_message(sender.get_fd(), ERR_NONICKNAMEGIVEN());
        return;
    } else if (is_nickname_in_use(param)) {
        server.send_message(sender.get_fd(), ERR_NICKNAMEINUSE(param));
        return;
    }
    std::string old_nick = sender.get_nickname();
    update_nickname(sender, param);
    broadcast_nickname_change(sender.get_fd(), old_nick, param);
}

void CommandsManager::user(Commands &commands, const Command &cmd) {
    const std::vector<std::string>& params = cmd.parameters;
    Client &client = commands.get_sender();

    // Debug print to verify parameters
    std::cout << "Parameters size: " << params.size() << std::endl;

    if (params.size() < 4) {
        server.send_message(client.get_fd(), ERR_NEEDMOREPARAMS(cmd.command));
        return;
    }

    if (client.is_authenticated()) {
        std::string username = params[0];
        std::string realname = params[3];

        update_user_info(client, username, realname);
        send_welcome_messages(client);
        return;
    }

    std::string username = params[0];
    std::string realname = params[3];

    update_user_info(client, username, realname);
    send_welcome_messages(client);
}

void CommandsManager::mode(Commands &commands, const Command &cmd) {
    Client &client = commands.get_sender();

    server.send_message(client.get_fd(), RPL_MODEBASE(client.get_nickname(), client.get_username(), cmd.parameters[0]));
}

void CommandsManager::quit(Commands &commands, const Command &cmd) {
    (void)commands;
    (void)cmd;
}

void CommandsManager::kick(Commands &commands, const Command &cmd) {
    (void)commands;
    (void)cmd;
}

void CommandsManager::invite(Commands &commands, const Command &cmd) {
    (void)commands;
    (void)cmd;
}

void CommandsManager::topic(Commands &commands, const Command &cmd) {
    (void)commands;
    (void)cmd;
}

void CommandsManager::unknown(Commands &commands, const Command &cmd) {
    (void)commands;
    (void)cmd;
}

void CommandsManager::pass(Commands &commands, const Command &cmd) {
    Client &client = commands.get_sender();
    const std::string &pass = cmd.parameters.empty() ? "" : cmd.parameters[0];
    if (pass.empty()) {
        server.send_message(client.get_fd(), ERR_NEEDMOREPARAMS(cmd.command));
    } else if (client.password_matched(server)) {
        server.send_message(client.get_fd(), ERR_ALREADYREGISTERED(client.get_username()));
    } else if (pass != server.get_pass()) {
        server.send_message(client.get_fd(), ERR_PASSWDMISMATCH());
        server.send_message(client.get_fd(), ERROR(std::string("wrong password")));
        commands.set_fatal_error(true);
    } else {
        client.set_authentication(true);
    }
}

void CommandsManager::broadcast_message(const std::string &msg, int sender_fd) {
    for (ClientMap::iterator it = server._clients.begin(); it != server._clients.end(); ++it) {
        int client_fd = it->first;
        if (client_fd != sender_fd) {
            server._message_queues[client_fd].push(msg);
        }
    }
}

void CommandsManager::broadcast_nickname_change(int sender_fd, const std::string &old_nick, const std::string &new_nick) {
    const std::string msg = ":" + old_nick + " NICK " + new_nick + "\r\n";
    broadcast_message(msg, sender_fd);
}

void CommandsManager::send_welcome_messages(Client &client) {
    if (!client.is_authenticated()) return;

    server.send_message(client.get_fd(), RPL_WELCOME( client.get_username(), client.get_client_identifier()));
    server.send_message(client.get_fd(), RPL_YOURHOST(client.get_nickname()));
    server.send_message(client.get_fd(), RPL_CREATED(client.get_nickname()));
    server.send_message(client.get_fd(), RPL_MYINFO(client.get_nickname(), "", ""));
}

void CommandsManager::update_user_info(Client &client, const std::string &username, const std::string &realname) {
    client.set_username(username);
    client.set_hostname(client.get_fd());  // Pass the sender_fd as the socket descriptor
    client.set_realname(realname);
}

void CommandsManager::update_nickname(Client &client, const std::string &new_nick) {
    client.set_nickname(new_nick);
}

bool CommandsManager::is_nickname_in_use(const std::string &new_nick) {
    for (std::map<int, Client *>::iterator it = server._clients.begin(); it != server._clients.end(); ++it) {
        if (it->second->get_nickname() == new_nick) {
            return true;
        }
    }
    return false;
}
