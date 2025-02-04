#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include "Server.hpp"
#include "Client.hpp"

#include <string>

class Server;

struct UserLimit {
	bool limited = false;
	size_t limit;
};

class Channel {
	
	private:

		std::string         _name;          // Channel name (starts with # or &)
		std::string         _topic;         // Channel topic
		std::string         _key;           // Channel password (if mode +k is set)
		std::map<int, Client*> _operators;  // Channel operators (fd -> Client*)
		
		// Channel modes
		bool _inviteOnly;      // mode +i
		bool _topicRestricted; // mode +t
		bool _hasKey;          // mode +k
		UserLimit _userLimit;     // mode +l

	public:	

		Channel(const std::string& name, Client* creator);
		~Channel();

		std::map<int, Client*> _members;    // Channel members (fd -> Client*)
		
		// Basic operations
		bool addMember(Client* client);
		bool removeMember(Client* client);
		void broadcastMessage(const std::string& message, Client* sender = NULL);
		
		// Channel operator operations
		void setOperator(Client* client);
		bool isOperator(Client* client) const;
		bool isMember(Client* client) const;
		
		// Mode operations
		//if only operators are able to set modes, a setter function to do it
		// is not necessary, checkMode though, is necessary, because other entities
		// outside the Channel class may neeed to access this information   
		// void setMode(char mode, bool status);
		bool checkChannelModes(const char mode) const;
		bool checkUserModes(char mode, Client* client);
		
		// Getters
		const std::string& getName() const;
		const std::string& getTopic() const;
		const std::map<int, Client*>& getMembers() const;
		size_t getCurrentMembersCount() const;
		bool getUserLimit() const;
					
		// Required by subject
		bool kickMember(Client* operator_client, Client* target, const std::string& reason);
		bool inviteMember(Client* operator_client, Client* target, Channel* channel);
		bool setTopic(Client* client, const std::string& new_topic);

		//validation
		bool parseChannelName(const std::string& name) const;
};