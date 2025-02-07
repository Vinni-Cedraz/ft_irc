#include <string>
#include "Client.hpp"
#include "CommandsManager.hpp"
#include "ft_irc.h"
#include <iostream>

CommandsManager::CommandsManager(Server &server) : server(server) {}

void CommandsManager::execute(Commands &commands) {
    const std::list<Command>& cmd_list = commands.get_list();
    for (std::list<Command>::const_iterator it = cmd_list.begin(); it != cmd_list.end(); ++it) {
        const Command &cmd = *it;
        switch (cmd.type) {
            case PRIVMSG:
                // privmsg(commands, cmd);
                break;
            case JOIN:
                // join(commands, cmd);
                break;
            case NICK:
                nick(commands, cmd);
                break;
            case USER:
                user(commands, cmd);
                break;
            case QUIT:
                // quit(commands, cmd);
                break;
            case PASS:
                pass(commands, cmd);
                break;
            case KICK:
                // kick();
                break;
            case INVITE:
                // invite();
                break;
            case TOPIC:
                // topic();
                break;
            case MODE:
                // mode();
                break;
            default:
                break;
        }
    }
    // Clear the list after execution
    commands.clear();
}

// void CommandsManager::privmsg(Commands &commands, const Command &cmd) {
//     // Implementation of PRIVMSG command
// }
//
// void CommandsManager::join(Commands &commands, const Command &cmd) {
//     // Implementation of JOIN command
// }

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
        server.send_message(client.get_fd(), ERR_ALREADYREGISTERED(client.get_username()));
        return;
    }

    std::string username = params[0];
    std::string realname = params[3];

    update_user_info(client, username, realname);
    send_welcome_messages(client);
}

void CommandsManager::join(Commands &commands, const Command &cmd) {
    Channel* channel;
    Client sender = commands.get_sender();
    
    if (!server.checkForChannel(cmd.parameters[0])) {
        server.addNewChannel(new Channel(cmd.parameters[0], commands.get_sender()));
        // verify channel setting:
    } else {
        channel = server._channels[cmd.parameters[0]];
    }
    //need to transfer this to the MODe function
    // if (cmd.parameters[1][0] != '+' && cmd.parameters[1][0] != '-') {
        
    // }

    if (channel->checkChannelModes('l') && channel->getCurrentMembersCount() < channel->getUserLimit()) {
        channel->addMember(&commands.get_sender());     
    } else {
        server.send_message(commands.get_sender().get_fd(), ERR_CHANNELISFULL(cmd.parameters[0]));
    } 
    if (channel->checkChannelModes('i')) {
        if (check_invite(sender, channel)) {

        }
    } else {


    } else if (nbr of params) {
        
    } else if (if the channel has key, does the matches the channel key?) {
        
    }
    else {
        server._channels[cmd.parameters[0]]->addMember(&commands.get_sender());
    }
};

// void CommandsManager::quit(Commands &commands, const Command &cmd) {
//     // Implementation of QUIT command
// }

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
        send_welcome_messages(client);
    }
}


// TO DO

//If <target> is a channel that does not exist on the network, 
void CommandsManager::mode(Commands &commands, const Command &cmd) {
    Client &client = commands.get_sender();
    Server* server = client.getServer();
    
    if (!server->checkForChannel(cmd.parameters[1])) {
        server->send_message(client.get_fd(), ERR_NOSUCHCHANNEL(cmd.parameters[1]));
    }
    if (!server->_channels[cmd.parameters[1]]->isOperator(&client)) {
        server->send_message(client.get_fd(), ERR_CHANOPRIVSNEEDED(client.get_nickname(), cmd.parameters[1]));
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

    server.send_message(client.get_fd(), RPL_WELCOME(client.get_nickname(), client.get_username()));
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

bool CommandsManager::check_invite(Client& client, Channel* channel) {
    for (std::vector<Invites>::iterator it = client.invites.begin(); it != client.invites.end(); it++) {
        if (it->channel.getName() == channel->getName()) {
            if (channel->isOperator(&it->who_invited)) {
                return true;
            }
        }
    }
    server.send_message(client.get_fd() , ERR_CHANOPRIVSNEEDED(client.get_nickname(), channel->getName()));
    return false;
};

