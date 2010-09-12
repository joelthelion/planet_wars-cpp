// Copyright 2010 owners of the AI Challenge project
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at http://www.apache.org/licenses/LICENSE-2.0 . Unless
// required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
//
// Author: Jeff Cameron (jeff@jpcameron.com)
// ported to C++ by Albert Zeyer
//
// Plays a game of Planet Wars between two computer programs.

#include <string>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include "utils.h"
#include "game.h"

void KillClients(std::vector<Process>& clients) {
	for (size_t i = 0; i < clients.size(); ++i) {
		if (clients[i])
			clients[i].destroy();
	}
}

int main(int argc, char** args) {
	using namespace std;
	
	// Check the command-line arguments.
	if (argc < 5) {
		cerr << "ERROR: wrong number of command-line arguments." << endl;
		cerr << "USAGE: engine map_file_name max_turn_time "
			 << "max_num_turns log_filename player_one "
			 << "player_two [more_players]" << endl;
		return 1;
	}

	// Initialize the game. Load the map.
	std::string mapFilename = args[0];
	long maxTurnTime = atol(args[1]);
	int maxNumTurns = atoi(args[2]);
	std::string logFilename = args[3];

	Game game(mapFilename, maxNumTurns, 0, logFilename);
	if (game.Init() == 0) {
		cerr << "ERROR: failed to start game. map: " << mapFilename << endl;
		return 1;
	}

	// Start the client programs (players).
	std::vector<Process> clients;
	for (int i = 4; i < argc; ++i) {
		std::string command = args[i];
		Process client;
		client.run(command);
		if (!client) {
			KillClients(clients);
			cerr << "ERROR: failed to start client: " << command << endl;
			return 1;
		}
		clients.push_back(client);
	}
	
	std::vector<bool> isAlive(clients.size());
	for (size_t i = 0; i < clients.size(); ++i) {
		isAlive[i] = (bool)clients[i];
	}
	
	int numTurns = 0;
	// Enter the main game loop.
	while (game.Winner() < 0) {
		// Send the game state to the clients.
		//cout << "The game state:" << endl;
		//cout << game.toString() << endl;
		for (size_t i = 0; i < clients.size(); ++i) {
			if (!clients[i] || !game.IsAlive(i + 1)) continue;
			
			std::string message = game.PovRepresentation(i + 1) + "go\n";
			try {
				clients[i] << message << flush;
				game.WriteLogMessage("engine > player" + to_string(i + 1) + ": " +
									 message);
			} catch (...) {
				clients[i].reset();
			}
		}
		
		// Get orders from the clients.
		std::vector<bool> clientDone(clients.size(), false);
		long startTime = currentTimeMillis();
		for (size_t i = 0; i < clients.size(); ++i) {
			if (!isAlive[i] || !game.IsAlive(i + 1) || clientDone[i]) {
				clientDone[i] = true;
				continue;
			}
			try {
				std::string line;
				while(true) {
					long dt = currentTimeMillis() - startTime;
					if(dt > maxTurnTime) break;
					if(!clients[i].readLine(line, maxTurnTime - dt)) break;

					line = ToLower(TrimSpaces(line));
					//cerr << "P" << (i+1) << ": " << line << endl;
					game.WriteLogMessage("player" + to_string(i + 1) + " > engine: " + line);
					if (line == "go")
						clientDone[i] = true;
					else
						game.IssueOrder(i + 1, line);
				}
			} catch (...) {
				cerr << "WARNING: player " << (i+1) << " crashed." << endl;
				clients[i].destroy();
				game.DropPlayer(i + 1);
				isAlive[i] = false;
			}
		}
		for (size_t i = 0; i < clients.size(); ++i) {
			if (!isAlive[i] || !game.IsAlive(i + 1)) continue;
			if (clientDone[i]) continue;

			cerr << "WARNING: player " << (i+1) << " timed out." << endl;
			clients[i].destroy();
			game.DropPlayer(i + 1);
			isAlive[i] = false;
		}
		++numTurns;
		cerr << "Turn " << numTurns << endl;
		game.DoTimeStep();
	}
	
	KillClients(clients);
	
	if (game.Winner() > 0) {
		cerr << "Player " << game.Winner() << " Wins!" << endl;
	} else {
		cerr << "Draw!" << endl;
	}
	
	cout << game.GamePlaybackString() << endl;
}

